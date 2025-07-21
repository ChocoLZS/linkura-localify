package io.github.chocolzs.linkura.localify.ui.overlay

import io.github.chocolzs.linkura.localify.ipc.DuplexSocketServer
import io.github.chocolzs.linkura.localify.ipc.MessageRouter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import android.app.Service
import android.content.Intent
import android.graphics.PixelFormat
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

class CameraOverlayService : Service(), LifecycleOwner, SavedStateRegistryOwner {
    companion object {
        private const val TAG = "CameraOverlayService"
    }

    private var windowManager: WindowManager? = null
    private var overlayView: View? = null
    private val handler = Handler(Looper.getMainLooper())
    private var cameraInfoState by mutableStateOf<CameraInfo?>(null)
    
    private val socketServer: DuplexSocketServer by lazy { DuplexSocketServer.getInstance() }
    private val messageRouter: MessageRouter by lazy { MessageRouter() }
    
    private val socketServerHandler = object : DuplexSocketServer.MessageHandler {
        override fun onMessageReceived(type: MessageType, payload: ByteArray) {
            messageRouter.routeMessage(type, payload)
        }

        override fun onClientConnected() {
        }

        override fun onClientDisconnected() {
        }
    }
    
    private val cameraDataHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val cameraData = CameraData.parseFrom(payload)
                handler.post {
                    updateCameraInfoFromProtobuf(cameraData)
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error handling camera data", e)
                false
            }
        }
    }

    private val overlayRequestHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val request = CameraOverlayRequest.parseFrom(payload)
                handler.post {
                    val overlayControl = OverlayControl.newBuilder()
                        .setAction(OverlayAction.START_OVERLAY)
                        .build()
                    socketServer.sendMessage(MessageType.OVERLAY_CONTROL, overlayControl)
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error handling CameraOverlayRequest", e)
                false
            }
        }
    }



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

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "CameraOverlayService onCreate")
        
        // Initialize lifecycle components
        savedStateRegistryController = SavedStateRegistryController.create(this)
        lifecycleRegistry = LifecycleRegistry(this)

        // Restore saved state and move to CREATED state
        savedStateRegistryController.performRestore(null)
        lifecycleRegistry.currentState = Lifecycle.State.CREATED

        try {
            createOverlay()
            setupSocketServer()
            if (socketServer.isConnected()) {
                // Send start overlay command to enable camera data loop
                val overlayControl = OverlayControl.newBuilder()
                    .setAction(OverlayAction.START_OVERLAY)
                    .build()
                socketServer.sendMessage(MessageType.OVERLAY_CONTROL, overlayControl)
            }
            // Move to STARTED state after successful initialization
            lifecycleRegistry.currentState = Lifecycle.State.STARTED
        } catch (e: Exception) {
            Log.e(TAG, "Error in onCreate", e)
            throw e
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "onStartCommand called with intent: $intent, flags: $flags, startId: $startId")
        return START_STICKY
    }
    
    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "CameraOverlayService onDestroy")
        
        try {
            lifecycleRegistry.currentState = Lifecycle.State.DESTROYED

            // Send stop overlay command before shutting down
            if (socketServer.isConnected()) {
                val overlayControl = OverlayControl.newBuilder()
                    .setAction(OverlayAction.STOP_OVERLAY)
                    .build()
                socketServer.sendMessage(MessageType.OVERLAY_CONTROL, overlayControl)
            }
            
            // Clean up socket server
            socketServer.removeMessageHandler(socketServerHandler)
            messageRouter.clearHandlers(MessageType.CAMERA_DATA)
            messageRouter.clearHandlers(MessageType.CAMERA_OVERLAY_REQUEST)

            overlayView?.let {
                windowManager?.removeView(it)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in onDestroy", e)
        }
    }

    private fun setupSocketServer() {
        // Register message handlers
        messageRouter.registerHandler(MessageType.CAMERA_DATA, cameraDataHandler)
        messageRouter.registerHandler(MessageType.CAMERA_OVERLAY_REQUEST, overlayRequestHandler)
        
        // Add server handler and start server
        socketServer.addMessageHandler(socketServerHandler)
        if (socketServer.startServer()) {
            Log.i(TAG, "Duplex socket server started for camera overlay")
        } else {
            Log.e(TAG, "Failed to start duplex socket server for camera overlay")
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

            params.gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
            params.x = 0
            params.y = 100
            overlayView = ComposeView(this).apply {

                // Set up ViewTreeLifecycleOwner and ViewTreeSavedStateRegistryOwner
                this.setViewTreeLifecycleOwner(this@CameraOverlayService)
                this.setViewTreeSavedStateRegistryOwner(this@CameraOverlayService)
                setContent {
                    CameraInfoOverlay()
                }
            }
            windowManager?.addView(overlayView, params)
        } catch (e: Exception) {
            Log.e(TAG, "Error creating overlay", e)
            throw e
        }
    }

    private fun updateCameraInfoFromProtobuf(cameraData: io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.CameraData) {
        try {
            val cameraInfo = CameraInfo(
                isValid = cameraData.isValid,
                position = Vector3(
                    x = cameraData.position.x,
                    y = cameraData.position.y,
                    z = cameraData.position.z
                ),
                rotation = Quaternion(
                    x = cameraData.rotation.x,
                    y = cameraData.rotation.y,
                    z = cameraData.rotation.z,
                    w = cameraData.rotation.w
                ),
                fov = cameraData.fov,
                mode = cameraData.mode,
                sceneType = cameraData.sceneType
            )

            cameraInfoState = cameraInfo
            Log.v(TAG, "Camera info state updated successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error updating camera info from protobuf", e)
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

}
