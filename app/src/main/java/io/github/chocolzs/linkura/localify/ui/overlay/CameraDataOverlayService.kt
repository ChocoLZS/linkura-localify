package io.github.chocolzs.linkura.localify.ui.overlay

import android.graphics.PixelFormat
import android.os.Build
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.Toast
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
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.ipc.MessageRouter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme
import kotlinx.serialization.Serializable

class CameraDataOverlayService(private val parentService: OverlayService) {
    companion object {
        private const val TAG = "CameraDataOverlayService"
    }
    
    private var overlayView: View? = null
    private var cameraInfoState by mutableStateOf<CameraInfo?>(null)
    private var isVisible = false
    
    // State machine for toast notifications
    private var hasShownToast = false
    private var lastWasConnecting = false
    
    // Touch handling for dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f
    
    private val cameraDataHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val cameraData = CameraData.parseFrom(payload)
                parentService.getHandlerInstance().post {
                    updateCameraInfoFromProtobuf(cameraData)
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error handling camera data", e)
                false
            }
        }
    }

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
        val sceneType: Int,
        val isConnecting: Boolean = false
    )

    init {
        setupMessageHandler()
    }

    private fun setupMessageHandler() {
        parentService.getAidlService()?.registerMessageHandler(MessageType.CAMERA_DATA, cameraDataHandler)
    }

    fun show() {
        if (isVisible) return
        
        try {
            createOverlay()
            isVisible = true
            
            // Send camera data request to Xposed clients to start camera data loop
            val overlayControl = OverlayControl.newBuilder()
                .setAction(OverlayAction.START_CAMERA_INFO_OVERLAY)
                .build()
            parentService.getAidlService()?.sendMessage(MessageType.OVERLAY_CONTROL_CAMERA_INFO, overlayControl)
        } catch (e: Exception) {
            Log.e(TAG, "Error showing camera overlay", e)
        }
    }

    fun hide() {
        if (!isVisible) return
        
        try {
            // Send stop camera info overlay command
            val overlayControl = OverlayControl.newBuilder()
                .setAction(OverlayAction.STOP_CAMERA_INFO_OVERLAY)
                .build()
            parentService.getAidlService()?.sendMessage(MessageType.OVERLAY_CONTROL_CAMERA_INFO, overlayControl)
            
            overlayView?.let { view ->
                parentService.getWindowManagerInstance()?.removeView(view)
            }
            overlayView = null
            isVisible = false
            parentService.resetCameraOverlayState()
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding camera overlay", e)
        }
    }

    private fun createOverlay() {
        val windowManager = parentService.getWindowManagerInstance()
        if (windowManager == null) {
            Log.e(TAG, "WindowManager is null, cannot create overlay")
            return
        }

        val layoutFlag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            WindowManager.LayoutParams.TYPE_PHONE
        }

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
        params.x = 100
        params.y = 200
        
        overlayView = ComposeView(parentService).apply {
            setViewTreeLifecycleOwner(parentService.getLifecycleOwnerInstance())
            setViewTreeSavedStateRegistryOwner(parentService.getSavedStateRegistryOwnerInstance())
            setContent {
                LocalifyTheme {
                    CameraInfoOverlay()
                }
            }
            
            // Add touch listener for dragging
            setOnTouchListener { v, event ->
                when (event.action) {
                    MotionEvent.ACTION_DOWN -> {
                        initialX = params.x
                        initialY = params.y
                        initialTouchX = event.rawX
                        initialTouchY = event.rawY
                        true
                    }
                    MotionEvent.ACTION_MOVE -> {
                        params.x = initialX + (event.rawX - initialTouchX).toInt()
                        params.y = initialY + (event.rawY - initialTouchY).toInt()
                        windowManager.updateViewLayout(v, params)
                        true
                    }
                    else -> false
                }
            }
        }
        windowManager.addView(overlayView, params)
    }

    private fun updateCameraInfoFromProtobuf(cameraData: CameraData) {
        try {
            val isConnecting = cameraData.hasIsConnecting() && cameraData.isConnecting
            
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
                sceneType = cameraData.sceneType,
                isConnecting = isConnecting
            )

            // State machine logic for toast notifications
            handleStateTransition(cameraInfo)

            cameraInfoState = cameraInfo
        } catch (e: Exception) {
            Log.e(TAG, "Error updating camera info from protobuf", e)
        }
    }

    private fun handleStateTransition(cameraInfo: CameraInfo) {
        // Reset state when transitioning from connecting
        if (lastWasConnecting && !cameraInfo.isConnecting) {
            hasShownToast = false
            Log.d(TAG, "Reset toast state: transitioning from connecting")
        }
        
        // Check conditions for showing toast
        if (!hasShownToast && 
            cameraInfo.isValid && 
            !cameraInfo.isConnecting &&
            cameraInfo.sceneType == 2 && // WITH_LIVE = 2
            cameraInfo.mode != 0) { // mode != SYSTEM_CAMERA (0)
            
            showWarningToast()
            hasShownToast = true
            Log.d(TAG, "Warning toast shown for WITH_LIVE with non-SYSTEM_CAMERA mode")
        }
        
        lastWasConnecting = cameraInfo.isConnecting
    }

    private fun showWarningToast() {
        parentService.getHandlerInstance().post {
            Toast.makeText(
                parentService,
                parentService.getString(R.string.overlay_toolbar_crash_warning),
                Toast.LENGTH_LONG
            ).show()
        }
    }

    @Composable
    private fun CameraInfoOverlay() {
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
                    if (info.isConnecting) {
                        Text(
                            text = "Connecting...",
                            color = Color.Gray,
                            fontSize = 10.sp
                        )
                    } else if (info.isValid) {
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
            0 -> "SYSTEM_CAMERA"
            1 -> "FREE"
            2 -> "FIRST_PERSON"
            3 -> "FOLLOW"
            else -> "UNKNOWN"
        }
    }

    private fun getSceneString(sceneType: Int): String {
        return when (sceneType) {
            0 -> "NONE"
            1 -> "FES_LIVE"
            2 -> "WITH_LIVE"
            3 -> "STORY"
            else -> "UNKNOWN"
        }
    }

    fun destroy() {
        hide()
        parentService.getAidlService()?.unregisterMessageHandler(MessageType.CAMERA_DATA, cameraDataHandler)
    }
}