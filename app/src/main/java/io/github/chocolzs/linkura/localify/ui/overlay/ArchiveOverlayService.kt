package io.github.chocolzs.linkura.localify.ui.overlay

import android.graphics.PixelFormat
import android.os.Build
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.ipc.MessageRouter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme
import kotlin.math.max
import kotlin.math.min

class ArchiveOverlayService(private val parentService: OverlayService) {
    companion object {
        private const val TAG = "ArchiveOverlayService"
    }
    
    private var overlayView: View? = null
    private var isVisible = false
    private var archiveDuration by mutableStateOf(0L) // Duration in milliseconds
    private var currentPosition by mutableStateOf(0f) // Current position as percentage (0.0 to 1.0)
    private var isDragging by mutableStateOf(false)
    private var tempPosition by mutableStateOf(0f) // Temporary position while dragging
    private var lastDraggedPosition by mutableStateOf(0f) // Remember last dragged position
    
    // Touch handling for window dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f
    
    private val archiveInfoHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val archiveInfo = ArchiveInfo.parseFrom(payload)
                parentService.getHandlerInstance().post {
                    archiveDuration = archiveInfo.duration
                    Log.d(TAG, "Received archive info response: duration=${archiveDuration}ms")
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error handling archive info response", e)
                false
            }
        }
    }

    init {
        setupMessageHandler()
    }

    private fun setupMessageHandler() {
        parentService.getMessageRouterInstance()?.registerHandler(MessageType.ARCHIVE_INFO, archiveInfoHandler)
    }

    fun show() {
        if (isVisible) return
        
        try {
            // Request archive info first
            requestArchiveInfo()
            createOverlay()
            isVisible = true
        } catch (e: Exception) {
            Log.e(TAG, "Error showing archive overlay", e)
        }
    }

    fun hide() {
        if (!isVisible) return
        
        try {
            overlayView?.let { view ->
                parentService.getWindowManagerInstance()?.removeView(view)
            }
            overlayView = null
            isVisible = false
            
            // Notify parent service to reset button state
            parentService.resetArchiveOverlayState()
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding archive overlay", e)
        }
    }

    private fun requestArchiveInfo() {
        try {
            val archiveInfo = ArchiveInfo.newBuilder().build()
            Log.d(TAG, "Requesting archive info from Xposed clients")
            parentService.sendMessage(MessageType.ARCHIVE_INFO.number, archiveInfo.toByteArray())
        } catch (e: Exception) {
            Log.e(TAG, "Error requesting archive info", e)
        }
    }

    private fun setArchivePosition(seconds: Float) {
        try {
            val request = ArchivePositionSetRequest.newBuilder()
                .setSeconds(seconds)
                .build()
            Log.d(TAG, "Sending archive position set request: ${seconds}s")
            parentService.sendMessage(MessageType.ARCHIVE_POSITION_SET_REQUEST.number, request.toByteArray())
            Log.d(TAG, "Archive position set to: ${seconds}s")
            
            // Save the last dragged position and hide the overlay after setting position
            lastDraggedPosition = currentPosition
            hide() // Auto-hide after sending request
        } catch (e: Exception) {
            Log.e(TAG, "Error setting archive position", e)
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

        params.gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
        params.x = 0
        params.y = 200
        
        overlayView = ComposeView(parentService).apply {
            setViewTreeLifecycleOwner(parentService.getLifecycleOwnerInstance())
            setViewTreeSavedStateRegistryOwner(parentService.getSavedStateRegistryOwnerInstance())
            setContent {
                LocalifyTheme {
                    ArchiveProgressOverlay()
                }
            }
            
            // Add touch listener for window dragging (avoid progress bar area)
            setOnTouchListener { v, event ->
                val isInProgressBarArea = event.y > 90 && event.y < 120 && archiveDuration > 0
                
                if (isInProgressBarArea) {
                    // Let progress bar handle the touch event
                    false
                } else {
                    // Handle window dragging
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
                            params.y = initialY - (event.rawY - initialTouchY).toInt()
                            windowManager.updateViewLayout(v, params)
                            true
                        }
                        else -> false
                    }
                }
            }
        }
        windowManager.addView(overlayView, params)
    }

    @Composable
    private fun ArchiveProgressOverlay() {
        if (archiveDuration == 0L) {
            // Show message when no archive is running
            Box(
                modifier = Modifier
                    .background(
                        Color.Black.copy(alpha = 0.8f),
                        RoundedCornerShape(12.dp)
                    )
                    .padding(16.dp)
            ) {
                Text(
                    text = parentService.getString(R.string.overlay_archive_no_rendering_running),
                    color = Color.White,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Medium
                )
            }
        } else {
            // Show progress bar
            val durationSeconds = archiveDuration / 1000f
            val displayPosition = if (isDragging) tempPosition else currentPosition
            
            Box(
                modifier = Modifier
                    .background(
                        Color.Black.copy(alpha = 0.8f),
                        RoundedCornerShape(12.dp)
                    )
                    .padding(16.dp)
                    .width(300.dp)
            ) {
                Column {
                    // Title bar for dragging
                    Text(
                        text = "Archive Progress",
                        color = Color.White,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.padding(bottom = 8.dp)
                    )
                    
                    // Time display
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(
                            text = "00:00",
                            color = Color.White,
                            fontSize = 11.sp
                        )
                        
                        Column(
                            horizontalAlignment = Alignment.CenterHorizontally
                        ) {
                            if (isDragging) {
                                val seekSeconds = displayPosition * durationSeconds
                                Text(
                                    text = formatTime(seekSeconds.toLong()),
                                    color = Color.Yellow,
                                    fontSize = 11.sp,
                                    fontWeight = FontWeight.Bold
                                )
                            } else if (lastDraggedPosition > 0f) {
                                // Show last dragged position when not dragging
                                val lastSeconds = lastDraggedPosition * durationSeconds
                                Text(
                                    text = "Last: ${formatTime(lastSeconds.toLong())}",
                                    color = Color.Cyan,
                                    fontSize = 10.sp
                                )
                            }
                        }
                        
                        Text(
                            text = formatTime(durationSeconds.toLong()),
                            color = Color.White,
                            fontSize = 11.sp
                        )
                    }
                    
                    Spacer(modifier = Modifier.height(8.dp))
                    
                    // Progress bar
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(24.dp)
                            .pointerInput(Unit) {
                                detectDragGestures(
                                    onDragStart = { offset ->
                                        isDragging = true
                                        val newPosition = (offset.x / size.width).coerceIn(0f, 1f)
                                        tempPosition = newPosition
                                    },
                                    onDrag = { change, offset ->
                                        val newPosition = ((change.position.x) / size.width).coerceIn(0f, 1f)
                                        tempPosition = newPosition
                                    },
                                    onDragEnd = {
                                        isDragging = false
                                        currentPosition = tempPosition
                                        val seekSeconds = tempPosition * durationSeconds
                                        setArchivePosition(seekSeconds)
                                    }
                                )
                            }
                    ) {
                        // Background track
                        Box(
                            modifier = Modifier
                                .fillMaxSize()
                                .background(
                                    Color.Gray.copy(alpha = 0.3f),
                                    RoundedCornerShape(12.dp)
                                )
                        )
                        
                        // Progress track
                        Box(
                            modifier = Modifier
                                .fillMaxHeight()
                                .fillMaxWidth(displayPosition)
                                .background(
                                    if (isDragging) Color.Yellow else Color.Blue,
                                    RoundedCornerShape(12.dp)
                                )
                        )
                        
                        // Thumb
                        Box(
                            modifier = Modifier
                                .size(24.dp)
                                .offset(x = (300.dp - 16.dp - 24.dp) * displayPosition)
                                .background(
                                    if (isDragging) Color.Yellow else Color.White,
                                    RoundedCornerShape(12.dp)
                                )
                                .align(Alignment.CenterStart)
                        )
                    }
                }
            }
        }
    }

    private fun formatTime(totalSeconds: Long): String {
        val hours = totalSeconds / 3600
        val minutes = (totalSeconds % 3600) / 60
        val seconds = totalSeconds % 60
        
        return if (hours > 0) {
            String.format("%d:%02d:%02d", hours, minutes, seconds)
        } else {
            String.format("%02d:%02d", minutes, seconds)
        }
    }

    fun destroy() {
        hide()
        parentService.getMessageRouterInstance()?.clearHandlers(MessageType.ARCHIVE_INFO)
    }
}