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
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.ipc.DuplexSocketServer
import io.github.chocolzs.linkura.localify.ipc.MessageRouter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*

class OverlayService : Service(), LifecycleOwner, SavedStateRegistryOwner {
    companion object {
        private const val TAG = "OverlayService"
    }

    private var windowManager: WindowManager? = null
    private var toolbarView: View? = null
    private val handler = Handler(Looper.getMainLooper())
    
    // Child services
    private var cameraOverlayService: CameraDataOverlayService? = null
    private var archiveOverlayService: ArchiveOverlayService? = null
    
    // UI state
    private var isCameraOverlayVisible by mutableStateOf(false)
    private var isArchiveOverlayVisible by mutableStateOf(false)
    
    // Socket communication
    private val socketServer: DuplexSocketServer by lazy { DuplexSocketServer.getInstance() }
    private val messageRouter: MessageRouter by lazy { MessageRouter() }
    
    // Touch handling for dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f
    
    // Lifecycle components
    private lateinit var lifecycleRegistry: LifecycleRegistry
    private lateinit var savedStateRegistryController: SavedStateRegistryController

    override val lifecycle: Lifecycle
        get() = lifecycleRegistry

    override val savedStateRegistry: SavedStateRegistry
        get() = savedStateRegistryController.savedStateRegistry

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "OverlayService onCreate")
        
        // Initialize lifecycle components
        savedStateRegistryController = SavedStateRegistryController.create(this)
        lifecycleRegistry = LifecycleRegistry(this)

        // Restore saved state and move to CREATED state
        savedStateRegistryController.performRestore(null)
        lifecycleRegistry.currentState = Lifecycle.State.CREATED

        try {
            createToolbarOverlay()
            setupSocketServer()
            
            // Create child services
            cameraOverlayService = CameraDataOverlayService(this)
            archiveOverlayService = ArchiveOverlayService(this)
            
            // Move to STARTED state after successful initialization
            lifecycleRegistry.currentState = Lifecycle.State.STARTED
        } catch (e: Exception) {
            Log.e(TAG, "Error in onCreate", e)
            throw e
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        Log.d(TAG, "onStartCommand called")
        return START_STICKY
    }
    
    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "OverlayService onDestroy")
        
        try {
            lifecycleRegistry.currentState = Lifecycle.State.DESTROYED

            // Clean up child services
            cameraOverlayService?.destroy()
            archiveOverlayService?.destroy()
            
            // Clean up socket server
            socketServer.removeMessageHandler(socketServerHandler)
            messageRouter.clearHandlers(MessageType.CAMERA_OVERLAY_REQUEST)

            toolbarView?.let {
                windowManager?.removeView(it)
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error in onDestroy", e)
        }
    }

    private val socketServerHandler = object : DuplexSocketServer.MessageHandler {
        override fun onMessageReceived(type: MessageType, payload: ByteArray) {
            messageRouter.routeMessage(type, payload)
        }

        override fun onClientConnected() {
            Log.d(TAG, "Socket client connected")
        }

        override fun onClientDisconnected() {
            Log.d(TAG, "Socket client disconnected")
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
                    socketServer.sendMessage(MessageType.OVERLAY_CONTROL_GENERAL, overlayControl)
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error handling CameraOverlayRequest", e)
                false
            }
        }
    }

    private fun setupSocketServer() {
        // Register message handlers
        messageRouter.registerHandler(MessageType.CAMERA_OVERLAY_REQUEST, overlayRequestHandler)
        
        // Add server handler and start server
        socketServer.addMessageHandler(socketServerHandler)
        if (socketServer.startServer()) {
            Log.i(TAG, "Duplex socket server started for main overlay")
        } else {
            Log.e(TAG, "Failed to start duplex socket server for main overlay")
        }
    }

    private fun createToolbarOverlay() {
        Log.d(TAG, "createToolbarOverlay called")
        try {
            windowManager = getSystemService(WINDOW_SERVICE) as WindowManager

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

            params.gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
            params.x = 0
            params.y = 100
            
            toolbarView = ComposeView(this).apply {
                setViewTreeLifecycleOwner(this@OverlayService)
                setViewTreeSavedStateRegistryOwner(this@OverlayService)
                setContent {
                    OverlayToolbar()
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
                            windowManager?.updateViewLayout(v, params)
                            true
                        }
                        else -> false
                    }
                }
            }
            windowManager?.addView(toolbarView, params)
        } catch (e: Exception) {
            Log.e(TAG, "Error creating toolbar overlay", e)
            throw e
        }
    }

    @Composable
    private fun OverlayToolbar() {
        Box(
            modifier = Modifier
                .background(
                    Color.Black.copy(alpha = 0.8f),
                    RoundedCornerShape(20.dp)
                )
                .padding(horizontal = 12.dp, vertical = 8.dp)
        ) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Camera info button
                IconButton(
                    onClick = {
                        toggleCameraOverlay()
                    },
                    modifier = Modifier
                        .size(40.dp)
                        .background(
                            if (isCameraOverlayVisible) Color.Blue.copy(alpha = 0.3f) else Color.Transparent,
                            RoundedCornerShape(8.dp)
                        )
                ) {
                    Icon(
                        imageVector = Icons.Default.Info,
                        contentDescription = "Camera Info",
                        tint = Color.White,
                        modifier = Modifier.size(20.dp)
                    )
                }
                
                // Archive progress button
                IconButton(
                    onClick = {
                        toggleArchiveOverlay()
                    },
                    modifier = Modifier
                        .size(40.dp)
                        .background(
                            if (isArchiveOverlayVisible) Color.Blue.copy(alpha = 0.3f) else Color.Transparent,
                            RoundedCornerShape(8.dp)
                        )
                ) {
                    Icon(
                        imageVector = Icons.Default.PlayArrow,
                        contentDescription = "Archive Progress",
                        tint = Color.White,
                        modifier = Modifier.size(20.dp)
                    )
                }
                
                // Menu/Close button
                IconButton(
                    onClick = {
                        stopSelf()
                    },
                    modifier = Modifier.size(40.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.MoreVert,
                        contentDescription = "Close",
                        tint = Color.White,
                        modifier = Modifier.size(20.dp)
                    )
                }
            }
        }
    }

    private fun toggleCameraOverlay() {
        isCameraOverlayVisible = !isCameraOverlayVisible
        if (isCameraOverlayVisible) {
            cameraOverlayService?.show()
        } else {
            cameraOverlayService?.hide()
        }
    }

    private fun toggleArchiveOverlay() {
        isArchiveOverlayVisible = !isArchiveOverlayVisible
        if (isArchiveOverlayVisible) {
            archiveOverlayService?.show()
        } else {
            archiveOverlayService?.hide()
        }
    }

    fun getSocketServerInstance(): DuplexSocketServer = socketServer
    fun getMessageRouterInstance(): MessageRouter = messageRouter
    fun getHandlerInstance(): Handler = handler
    fun getWindowManagerInstance(): WindowManager? = windowManager
    
    // Method for child services to update parent state
    fun resetArchiveOverlayState() {
        isArchiveOverlayVisible = false
    }
    fun getLifecycleOwnerInstance(): LifecycleOwner = this
    fun getSavedStateRegistryOwnerInstance(): SavedStateRegistryOwner = this
}