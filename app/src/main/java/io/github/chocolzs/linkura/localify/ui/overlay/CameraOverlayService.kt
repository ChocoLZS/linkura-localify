package io.github.chocolzs.linkura.localify.ui.overlay

import android.app.Service
import android.content.Intent
import android.graphics.PixelFormat
import android.net.LocalServerSocket
import android.net.LocalSocket
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.mainUtils.json
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.float
import kotlinx.serialization.json.int
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.IOException

class CameraOverlayService : Service(), LifecycleOwner, SavedStateRegistryOwner {
    companion object {
        private const val TAG = "CameraOverlayService"
        private const val SOCKET_ADDRESS = "camera_data_socket"
    }

    private var windowManager: WindowManager? = null
    private var overlayView: View? = null
    private val handler = Handler(Looper.getMainLooper())
    private var cameraInfoState by mutableStateOf<CameraInfo?>(null)

    private var serverSocket: LocalServerSocket? = null
    private val socketListenerThread = Thread { startSocketListener() }


    // Lifecycle components
    private lateinit var lifecycleRegistry: LifecycleRegistry
    private lateinit var savedStateRegistryController: SavedStateRegistryController

    override val lifecycle: Lifecycle
        get() = lifecycleRegistry

    override val savedStateRegistry: SavedStateRegistry
        get() = savedStateRegistryController.savedStateRegistry

    @Serializable
    data class Vector3(val x: Float, val y: Float, val z: Float)

    @Serializable
    data class Quaternion(val x: Float, val y: Float, val z: Float, val w: Float)

    @Serializable
    data class CameraInfo(
        val isValid: Boolean,
        val position: Vector3,
        val rotation: Quaternion,
        val fov: Float,
        val mode: Int,
        val sceneType: Int
    )

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "onStartCommand called with intent: $intent, flags: $flags, startId: $startId")
        return START_STICKY
    }

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "Service onCreate called")

        // Initialize lifecycle components
        Log.d(TAG, "Initializing lifecycle components")
        savedStateRegistryController = SavedStateRegistryController.create(this)
        lifecycleRegistry = LifecycleRegistry(this)

        // Restore saved state and move to CREATED state
        savedStateRegistryController.performRestore(null)
        lifecycleRegistry.currentState = Lifecycle.State.CREATED
        Log.d(TAG, "Lifecycle components initialized")

        try {
            Log.d(TAG, "About to create overlay")
            createOverlay()
            Log.d(TAG, "Overlay created successfully")

            // Start listening for camera data on a local socket
            socketListenerThread.start()

            // Move to STARTED state after successful initialization
            lifecycleRegistry.currentState = Lifecycle.State.STARTED
            Log.d(TAG, "Lifecycle moved to STARTED state")

            Log.d(TAG, "Service onCreate completed successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error in onCreate", e)
            throw e
        }
    }

    private fun startSocketListener() {
        try {
            // Use abstract namespace socket
            serverSocket = LocalServerSocket(SOCKET_ADDRESS)
            Log.i(TAG, "Server socket listening at $SOCKET_ADDRESS")

            while (!Thread.currentThread().isInterrupted) {
                var clientSocket: LocalSocket? = null
                try {
                    clientSocket = serverSocket?.accept()
                    Log.i(TAG, "Client connected")

                    val reader = BufferedReader(InputStreamReader(clientSocket?.inputStream))
                    while (true) {
                        val jsonString = reader.readLine() ?: break // Connection closed
                        handler.post {
                            updateCameraInfoFromJson(jsonString)
                        }
                    }
                } catch (e: IOException) {
                    Log.w(TAG, "Client socket disconnected or error.", e)
                } finally {
                    try {
                        clientSocket?.close()
                    } catch (e: IOException) {
                        Log.e(TAG, "Error closing client socket", e)
                    }
                    Log.i(TAG, "Client disconnected")
                }
            }
        } catch (e: IOException) {
            Log.e(TAG, "Error creating or running server socket", e)
        } finally {
            try {
                serverSocket?.close()
            } catch (e: IOException) {
                Log.e(TAG, "Error closing server socket", e)
            }
            Log.i(TAG, "Server socket closed")
        }
    }


    private fun createOverlay() {
        Log.d(TAG, "createOverlay called")
        try {
            windowManager = getSystemService(WINDOW_SERVICE) as WindowManager
            Log.d(TAG, "WindowManager obtained")

            val layoutFlag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
            } else {
                WindowManager.LayoutParams.TYPE_PHONE
            }
            Log.d(TAG, "Layout flag determined: $layoutFlag")

            val params = WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                layoutFlag,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                        WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
                        WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
                PixelFormat.TRANSLUCENT
            )

            params.gravity = Gravity.TOP or Gravity.START
            params.x = 50
            params.y = 100
            Log.d(TAG, "Layout params configured")

            Log.d(TAG, "About to create ComposeView")
            overlayView = ComposeView(this).apply {
                Log.d(TAG, "ComposeView instantiated, setting up lifecycle")

                // Set up ViewTreeLifecycleOwner and ViewTreeSavedStateRegistryOwner
                this.setViewTreeLifecycleOwner(this@CameraOverlayService)
                this.setViewTreeSavedStateRegistryOwner(this@CameraOverlayService)
                Log.d(TAG, "ViewTree owners set")

                Log.d(TAG, "Setting content")
                setContent {
                    Log.d(TAG, "Setting CameraInfoOverlay content")
                    CameraInfoOverlay()
                }
            }
            Log.d(TAG, "ComposeView created and content set")

            Log.d(TAG, "About to add view to window manager")
            windowManager?.addView(overlayView, params)
            Log.d(TAG, "Overlay view added to window manager successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error creating overlay", e)
            throw e
        }
    }

    private fun updateCameraInfoFromJson(jsonString: String) {
        try {
            Log.v(TAG, "updateCameraInfoFromJson called with: $jsonString")
            val jsonElement = json.parseToJsonElement(jsonString).jsonObject

            val cameraInfo = CameraInfo(
                isValid = jsonElement["isValid"]?.jsonPrimitive?.boolean ?: false,
                position = Vector3(
                    x = jsonElement["position"]?.jsonObject?.get("x")?.jsonPrimitive?.float ?: 0f,
                    y = jsonElement["position"]?.jsonObject?.get("y")?.jsonPrimitive?.float ?: 0f,
                    z = jsonElement["position"]?.jsonObject?.get("z")?.jsonPrimitive?.float ?: 0f
                ),
                rotation = Quaternion(
                    x = jsonElement["rotation"]?.jsonObject?.get("x")?.jsonPrimitive?.float ?: 0f,
                    y = jsonElement["rotation"]?.jsonObject?.get("y")?.jsonPrimitive?.float ?: 0f,
                    z = jsonElement["rotation"]?.jsonObject?.get("z")?.jsonPrimitive?.float ?: 0f,
                    w = jsonElement["rotation"]?.jsonObject?.get("w")?.jsonPrimitive?.float ?: 1f
                ),
                fov = jsonElement["fov"]?.jsonPrimitive?.float ?: 60f,
                mode = jsonElement["mode"]?.jsonPrimitive?.int ?: 0,
                sceneType = jsonElement["sceneType"]?.jsonPrimitive?.int ?: 0
            )

            cameraInfoState = cameraInfo
            Log.v(TAG, "Camera info state updated successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing camera info JSON", e)
        }
    }

    @Composable
    private fun CameraInfoOverlay() {
        Log.d(TAG, "CameraInfoOverlay composable called")

        LaunchedEffect(Unit) {
            Log.d(TAG, "CameraInfoOverlay LaunchedEffect started - waiting for external updates")
        }

        Box(
            modifier = Modifier
                .background(
                    Color.Black.copy(alpha = 0.7f),
                    RoundedCornerShape(8.dp)
                )
                .padding(12.dp)
        ) {
            Column {
                Text(
                    text = "Camera Info",
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold
                )

                Spacer(modifier = Modifier.height(4.dp))

                cameraInfoState?.let { info ->
                    if (info.isValid) {
                        Text(
                            text = "Pos: (${String.format("%.2f", info.position.x)}, ${String.format("%.2f", info.position.y)}, ${String.format("%.2f", info.position.z)})",
                            color = Color.White,
                            fontSize = 10.sp
                        )
                        Text(
                            text = "Rot: (${String.format("%.2f", info.rotation.x)}, ${String.format("%.2f", info.rotation.y)}, ${String.format("%.2f", info.rotation.z)}, ${String.format("%.2f", info.rotation.w)})",
                            color = Color.White,
                            fontSize = 10.sp
                        )
                        Text(
                            text = "FOV: ${String.format("%.1f", info.fov)}",
                            color = Color.White,
                            fontSize = 10.sp
                        )
                        Text(
                            text = "Mode: ${getModeString(info.mode)}",
                            color = Color.White,
                            fontSize = 10.sp
                        )
                        Text(
                            text = "Scene: ${getSceneString(info.sceneType)}",
                            color = Color.White,
                            fontSize = 10.sp
                        )
                    } else {
                        Text(
                            text = "No Camera Data",
                            color = Color.Gray,
                            fontSize = 10.sp
                        )
                    }
                } ?: run {
                    Text(
                        text = "Connecting...",
                        color = Color.Gray,
                        fontSize = 10.sp
                    )
                }
            }
        }
    }

    private fun getModeString(mode: Int): String {
        return when (mode) {
            0 -> "FREE"
            1 -> "FIRST_PERSON"
            2 -> "FOLLOW"
            else -> "UNKNOWN"
        }
    }

    private fun getSceneString(sceneType: Int): String {
        return when (sceneType) {
            0 -> "NONE"
            1 -> "WITH_LIVE"
            2 -> "FES_LIVE"
            3 -> "STORY"
            else -> "UNKNOWN"
        }
    }


    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "Service onDestroy called")
        try {
            // Interrupt the listener thread and close the socket
            socketListenerThread.interrupt()
            serverSocket?.close()
            Log.d(TAG, "Socket listener stopped and socket closed")

            // Move lifecycle to DESTROYED state
            Log.d(TAG, "Moving lifecycle to DESTROYED state")
            lifecycleRegistry.currentState = Lifecycle.State.DESTROYED

            overlayView?.let {
                windowManager?.removeView(it)
                Log.d(TAG, "Overlay view removed")
            }

            Log.d(TAG, "Service cleanup completed")
        } catch (e: Exception) {
            Log.e(TAG, "Error in onDestroy", e)
        }
    }
}
