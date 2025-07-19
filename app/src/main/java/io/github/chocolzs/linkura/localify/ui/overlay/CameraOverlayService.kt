package io.github.chocolzs.linkura.localify.ui.overlay

import android.app.Service
import android.content.Intent
import android.graphics.PixelFormat
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.WindowManager
import android.widget.TextView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.chocolzs.linkura.localify.LinkuraHookMain
import io.github.chocolzs.linkura.localify.mainUtils.json
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.serialization.Serializable
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.boolean
import kotlinx.serialization.json.float
import kotlinx.serialization.json.int
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive

class CameraOverlayService : Service() {
    companion object {
        private const val TAG = "CameraOverlayService"
    }
    
    private var windowManager: WindowManager? = null
    private var overlayView: View? = null
    private val handler = Handler(Looper.getMainLooper())
    private var updateRunnable: Runnable? = null
    
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
        Log.d(TAG, "Service onCreate called")
        try {
            createOverlay()
            startCameraInfoUpdates()
            Log.d(TAG, "Service onCreate completed successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error in onCreate", e)
            throw e
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

            overlayView = ComposeView(this).apply {
                setContent {
                    CameraInfoOverlay()
                }
            }
            Log.d(TAG, "ComposeView created")

            windowManager?.addView(overlayView, params)
            Log.d(TAG, "Overlay view added to window manager")
        } catch (e: Exception) {
            Log.e(TAG, "Error creating overlay", e)
            throw e
        }
    }

    @Composable
    private fun CameraInfoOverlay() {
        var cameraInfoState by remember { mutableStateOf<CameraInfo?>(null) }
        
        LaunchedEffect(Unit) {
            Log.d(TAG, "CameraInfoOverlay LaunchedEffect started")
            while (true) {
                try {
                    val jsonString = LinkuraHookMain.getCameraInfoJson()
                    Log.v(TAG, "Got camera info JSON: $jsonString")
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
                    Log.v(TAG, "Camera info updated successfully")
                } catch (e: Exception) {
                    Log.e(TAG, "Error updating camera info", e)
                }
                delay(100) // Update every 100ms
            }
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
                        text = "Loading...",
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

    @OptIn(DelicateCoroutinesApi::class)
    private fun startCameraInfoUpdates() {
        Log.d(TAG, "startCameraInfoUpdates called")
        GlobalScope.launch {
            while (isActive) {
                // The compose view handles its own updates
                delay(1000)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "Service onDestroy called")
        try {
            overlayView?.let {
                windowManager?.removeView(it)
                Log.d(TAG, "Overlay view removed")
            }
            updateRunnable?.let {
                handler.removeCallbacks(it)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in onDestroy", e)
        }
    }
}