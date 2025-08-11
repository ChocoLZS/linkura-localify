package io.github.chocolzs.linkura.localify.ui.overlay

import android.app.Service
import android.content.Context
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
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.unit.IntOffset
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.KeyboardArrowLeft
import androidx.compose.material.icons.filled.PhotoCamera
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.unit.dp
import androidx.compose.ui.graphics.toArgb
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.ipc.DuplexSocketServer
import io.github.chocolzs.linkura.localify.ipc.MessageRouter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.ui.components.ColorPicker
import io.github.chocolzs.linkura.localify.ui.components.ConnectionStatusIndicator

import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme

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
    private var colorPickerOverlayService: ColorPickerOverlayService? = null
    private var cameraSensitivityOverlayService: CameraSensitivityOverlayService? = null
    
    // UI state
    private var isCameraOverlayVisible by mutableStateOf(false)
    private var isArchiveOverlayVisible by mutableStateOf(false)
    private var isColorPickerVisible by mutableStateOf(false)
    private var isCameraSensitivityVisible by mutableStateOf(false)
    private var isCameraMenuVisible by mutableStateOf(false)
    private var currentBackgroundColor by mutableStateOf(Color.Black)
    private var isToolbarCollapsed by mutableStateOf(false)
    private var isToolbarVisible by mutableStateOf(true)
    
    // Socket communication
    private val socketServer: DuplexSocketServer by lazy { DuplexSocketServer.getInstance() }
    private val messageRouter: MessageRouter by lazy { MessageRouter() }
    
    // Touch handling for dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f
    private var toolbarParams: WindowManager.LayoutParams? = null
    
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

        val sharedPrefs = getSharedPreferences("overlay_settings", Context.MODE_PRIVATE)
        val colorInt = sharedPrefs.getInt("background_color", Color.Black.toArgb())
        currentBackgroundColor = Color(colorInt)

        try {
            createToolbarOverlay()
            setupSocketServer()
            
            // Create child services
            cameraOverlayService = CameraDataOverlayService(this)
            archiveOverlayService = ArchiveOverlayService(this)
            colorPickerOverlayService = ColorPickerOverlayService(this)
            cameraSensitivityOverlayService = CameraSensitivityOverlayService(this)
            
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
            colorPickerOverlayService?.destroy()
            cameraSensitivityOverlayService?.destroy()
            
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

            // Start in center, will adjust position based on collapsed state
            params.gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
            params.x = 0
            params.y = 100
            
            toolbarParams = params
            
            toolbarView = ComposeView(this).apply {
                setViewTreeLifecycleOwner(this@OverlayService)
                setViewTreeSavedStateRegistryOwner(this@OverlayService)
                setContent {
                    LocalifyTheme {
                        OverlayToolbar()
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
        if (!isToolbarVisible) return
        
        // Main Box that contains both toolbar and secondary menu
        Box {
            if (isToolbarCollapsed) {
                // Collapsed state - show only expand button on the right side
                Box(
                    modifier = Modifier
                        .background(
                            Color.Black.copy(alpha = 0.8f),
                            RoundedCornerShape(16.dp)
                        )
                        .padding(8.dp)
                ) {
                    IconButton(
                        onClick = {
                            isToolbarCollapsed = false
                            updateToolbarPosition()
                        },
                        modifier = Modifier.size(32.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.KeyboardArrowLeft,
                            contentDescription = "Expand Toolbar",
                            tint = Color.White,
                            modifier = Modifier.size(18.dp)
                        )
                    }
                }
            } else {
                // Expanded state - show full toolbar
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // Hide button (smaller, centered at top)
                    Box(
                        modifier = Modifier
                            .background(
                                Color.Black.copy(alpha = 0.8f),
                                RoundedCornerShape(12.dp)
                            )
                            .padding(4.dp)
                    ) {
                        IconButton(
                            onClick = {
                                isToolbarVisible = false
                                // Update plugin side overlay state
                                OverlayManager.markOverlayAsHidden()
                            },
                            modifier = Modifier.size(24.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.Close,
                                contentDescription = "Hide Toolbar",
                                tint = Color.White,
                                modifier = Modifier.size(14.dp)
                            )
                        }
                    }
                    
                    // Main toolbar content
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
                            // Connection status indicator (left side)
                            ConnectionStatusIndicator(
                                size = 12f,
                                showDialog = false
                            )
                            
                            // Camera button (without nested secondary menu)
                            IconButton(
                                onClick = {
                                    toggleCameraMenu()
                                },
                                modifier = Modifier
                                    .size(40.dp)
                                    .background(
                                        if (isCameraMenuVisible || isCameraOverlayVisible || isCameraSensitivityVisible) 
                                            Color.Blue.copy(alpha = 0.3f) else Color.Transparent,
                                        RoundedCornerShape(8.dp)
                                    )
                            ) {
                                Icon(
                                    imageVector = Icons.Default.PhotoCamera,
                                    contentDescription = getString(R.string.overlay_camera_title),
                                    tint = Color.White,
                                    modifier = Modifier.size(20.dp)
                                )
                            }
                            
                            // Archive progress button
                            IconButton(
                                onClick = {
                                    toggleArchiveOverlay()
                                    isCameraMenuVisible = false
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
                            
                            // Color picker button with color indicator
                            Box(
                                modifier = Modifier
                                    .size(40.dp)
                                    .background(
                                        if (isColorPickerVisible) Color.Blue.copy(alpha = 0.3f) else Color.Transparent,
                                        RoundedCornerShape(8.dp)
                                    )
                            ) {
                                IconButton(
                                    onClick = {
                                        toggleColorPickerOverlay()
                                        isCameraMenuVisible = false
                                    },
                                    modifier = Modifier.fillMaxSize()
                                ) {
                                    Icon(
                                        imageVector = Icons.Default.Palette,
                                        contentDescription = "Background Color",
                                        tint = Color.White,
                                        modifier = Modifier.size(20.dp)
                                    )
                                }
                                
                                // Color indicator
                                Box(
                                    modifier = Modifier
                                        .align(Alignment.BottomEnd)
                                        .size(12.dp)
                                        .offset(x = 2.dp, y = 2.dp)
                                        .clip(RoundedCornerShape(2.dp))
                                        .background(currentBackgroundColor)
                                        .border(0.5.dp, Color.White, RoundedCornerShape(2.dp))
                                )
                            }
                            
                            // Collapse button (rightmost)
                            IconButton(
                                onClick = {
                                    isToolbarCollapsed = true
                                    isCameraMenuVisible = false
                                    updateToolbarPosition()
                                },
                                modifier = Modifier.size(40.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.KeyboardArrowRight,
                                    contentDescription = "Collapse Toolbar",
                                    tint = Color.White,
                                    modifier = Modifier.size(20.dp)
                                )
                            }
                        }
                    }
                    // Secondary menu - positioned as third element in Column
                    if (isCameraMenuVisible) {
                        CameraSecondaryMenu()
                    }
                }
            }
        }
    }

    @Composable
    private fun CameraSecondaryMenu(modifier: Modifier = Modifier) {
        Box(
            modifier = modifier
                .background(
                    Color.Black.copy(alpha = 0.8f),
                    RoundedCornerShape(8.dp)
                )
                .padding(4.dp),
            contentAlignment = Alignment.Center
        ) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(4.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Camera Info button
                IconButton(
                    onClick = {
                        toggleCameraOverlay()
                        isCameraMenuVisible = false
                    },
                    modifier = Modifier
                        .size(32.dp)
                        .background(
                            if (isCameraOverlayVisible) Color.Blue.copy(alpha = 0.3f) else Color.Transparent,
                            RoundedCornerShape(6.dp)
                        )
                ) {
                    Icon(
                        imageVector = Icons.Default.Info,
                        contentDescription = getString(R.string.overlay_camera_info),
                        tint = Color.White,
                        modifier = Modifier.size(16.dp)
                    )
                }

                // Camera Sensitivity button
                IconButton(
                    onClick = {
                        toggleCameraSensitivityOverlay()
                        isCameraMenuVisible = false
                    },
                    modifier = Modifier
                        .size(32.dp)
                        .background(
                            if (isCameraSensitivityVisible) Color.Blue.copy(alpha = 0.3f) else Color.Transparent,
                            RoundedCornerShape(6.dp)
                        )
                ) {
                    Icon(
                        imageVector = Icons.Default.Settings,
                        contentDescription = getString(R.string.overlay_camera_sensitivity),
                        tint = Color.White,
                        modifier = Modifier.size(16.dp)
                    )
                }
            }
        }
    }

    private fun toggleCameraMenu() {
        isCameraMenuVisible = !isCameraMenuVisible
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
    
    private fun toggleColorPickerOverlay() {
        isColorPickerVisible = !isColorPickerVisible
        if (isColorPickerVisible) {
            colorPickerOverlayService?.show(currentBackgroundColor)
        } else {
            colorPickerOverlayService?.hide()
        }
    }
    
    private fun toggleCameraSensitivityOverlay() {
        isCameraSensitivityVisible = !isCameraSensitivityVisible
        if (isCameraSensitivityVisible) {
            cameraSensitivityOverlayService?.show()
        } else {
            cameraSensitivityOverlayService?.hide()
        }
    }

    fun onColorChanged(color: Color) {
        currentBackgroundColor = color
        val sharedPrefs = getSharedPreferences("overlay_settings", Context.MODE_PRIVATE)
        with(sharedPrefs.edit()) {
            putInt("background_color", color.toArgb())
            apply()
        }

        // Send color change message via socket
        try {
            val colorMessage = CameraBackgroundColor.newBuilder()
                .setRed(color.red)
                .setGreen(color.green)
                .setBlue(color.blue)
                .setAlpha(color.alpha)
                .build()
            
            if (socketServer.isConnected()) {
                socketServer.sendMessage(MessageType.CAMERA_BACKGROUND_COLOR, colorMessage)
                Log.d(TAG, "Sent background color: R=${color.red}, G=${color.green}, B=${color.blue}, A=${color.alpha}")
            } else {
                Log.w(TAG, "Socket server not connected, cannot send color change")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending color change", e)
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

    fun resetColorPickerOverlayState() {
        isColorPickerVisible = false
    }

    fun resetCameraOverlayState() {
        isCameraOverlayVisible = false
    }
    
    fun resetCameraSensitivityOverlayState() {
        isCameraSensitivityVisible = false
    }
    
    private fun updateToolbarPosition() {
        toolbarParams?.let { params ->
            if (isToolbarCollapsed) {
                // Position collapsed toolbar on the right side
                params.gravity = Gravity.TOP or Gravity.END
                params.x = 0
                params.y = 200
            } else {
                // Position expanded toolbar in center
                params.gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
                params.x = 0
                params.y = 100
            }
            
            toolbarView?.let { view ->
                try {
                    windowManager?.updateViewLayout(view, params)
                } catch (e: Exception) {
                    Log.e(TAG, "Error updating toolbar position", e)
                }
            }
        }
    }

    fun getLifecycleOwnerInstance(): LifecycleOwner = this
    fun getSavedStateRegistryOwnerInstance(): SavedStateRegistryOwner = this
}