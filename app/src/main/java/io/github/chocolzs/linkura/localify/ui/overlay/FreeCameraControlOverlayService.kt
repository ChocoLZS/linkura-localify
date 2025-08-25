package io.github.chocolzs.linkura.localify.ui.overlay

import android.annotation.SuppressLint
import android.graphics.PixelFormat
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.ArrowForward
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.center
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme
import io.github.chocolzs.linkura.localify.ipc.LinkuraAidlClient
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.*

class FreeCameraControlOverlayService(private val parentService: OverlayService) {
    companion object {
        private const val TAG = "FreeCameraControlOverlayService"
        
        // Key codes for virtual keyboard
        private const val KEY_W = KeyEvent.KEYCODE_W
        private const val KEY_A = KeyEvent.KEYCODE_A
        private const val KEY_S = KeyEvent.KEYCODE_S
        private const val KEY_D = KeyEvent.KEYCODE_D
        private const val KEY_Q = KeyEvent.KEYCODE_Q
        private const val KEY_E = KeyEvent.KEYCODE_E
        private const val KEY_R = KeyEvent.KEYCODE_R
        private const val KEY_F = KeyEvent.KEYCODE_F
        private const val KEY_SPACE = KeyEvent.KEYCODE_SPACE
        private const val KEY_CTRL = KeyEvent.KEYCODE_CTRL_LEFT
        private const val KEY_UP = KeyEvent.KEYCODE_DPAD_UP
        private const val KEY_DOWN = KeyEvent.KEYCODE_DPAD_DOWN
        private const val KEY_LEFT = KeyEvent.KEYCODE_DPAD_LEFT
        private const val KEY_RIGHT = KeyEvent.KEYCODE_DPAD_RIGHT
    }
    
    private var leftControlView: View? = null
    private var rightControlView: View? = null
    private var closeButtonView: View? = null
    private var isVisible = false
    
    // WASD center button state
    private var isCenterButtonToggled by mutableStateOf(false)
    
    // Touch handling for dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f
    
    // Key press handling
    private val handler = Handler(Looper.getMainLooper())
    private val activeKeys = mutableSetOf<Int>()
    private val keyRepeatTasks = mutableMapOf<Int, Runnable>()
    
    // Virtual joystick state (for free camera mode)
    private var joystickCenterX by mutableFloatStateOf(0f)
    private var joystickCenterY by mutableFloatStateOf(0f)
    private var joystickOffsetX by mutableFloatStateOf(0f)
    private var joystickOffsetY by mutableFloatStateOf(0f)
    
    fun show() {
        if (isVisible) return
        
        // Check if device is in landscape mode
        val configuration = parentService.resources.configuration
        val isLandscape = configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
        
        if (!isLandscape) {
            // Show toast notification for non-landscape mode
            android.widget.Toast.makeText(
                parentService,
                "请切换到横屏模式使用虚拟控制器",
                android.widget.Toast.LENGTH_SHORT
            ).show()
            Log.d(TAG, "Free camera control overlay requires landscape mode")
            return
        }
        
        try {
            createLeftControlOverlay()
            createRightControlOverlay() 
            createCloseButtonOverlay()
            isVisible = true
            Log.d(TAG, "Free camera control overlay shown")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing free camera control overlay", e)
        }
    }

    fun hide() {
        if (!isVisible) return
        
        try {
            // Stop all active key repeats
            stopAllKeyRepeats()
            
            val windowManager = parentService.getWindowManagerInstance()
            leftControlView?.let { view ->
                windowManager?.removeView(view)
            }
            rightControlView?.let { view ->
                windowManager?.removeView(view)  
            }
            closeButtonView?.let { view ->
                windowManager?.removeView(view)
            }
            leftControlView = null
            rightControlView = null
            closeButtonView = null
            isVisible = false
            
            parentService.resetFreeCameraControlOverlayState()
            Log.d(TAG, "Free camera control overlay hidden")
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding free camera control overlay", e)
        }
    }

    private fun createLeftControlOverlay() {
        val windowManager = parentService.getWindowManagerInstance()
        if (windowManager == null) {
            Log.e(TAG, "WindowManager is null, cannot create left control overlay")
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

        params.gravity = Gravity.BOTTOM or Gravity.START
        params.x = 32
        params.y = 32
        
        leftControlView = ComposeView(parentService).apply {
            setViewTreeLifecycleOwner(parentService.getLifecycleOwnerInstance())
            setViewTreeSavedStateRegistryOwner(parentService.getSavedStateRegistryOwnerInstance())
            setContent {
                LocalifyTheme {
                    LeftControlSection()
                }
            }
        }
        windowManager.addView(leftControlView, params)
    }
    
    private fun createRightControlOverlay() {
        val windowManager = parentService.getWindowManagerInstance()
        if (windowManager == null) {
            Log.e(TAG, "WindowManager is null, cannot create right control overlay")
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

        params.gravity = Gravity.BOTTOM or Gravity.END
        params.x = 32
        params.y = 32
        
        rightControlView = ComposeView(parentService).apply {
            setViewTreeLifecycleOwner(parentService.getLifecycleOwnerInstance())
            setViewTreeSavedStateRegistryOwner(parentService.getSavedStateRegistryOwnerInstance())
            setContent {
                LocalifyTheme {
                    RightControlSection()
                }
            }
        }
        windowManager.addView(rightControlView, params)
    }
    
    private fun createCloseButtonOverlay() {
        val windowManager = parentService.getWindowManagerInstance()
        if (windowManager == null) {
            Log.e(TAG, "WindowManager is null, cannot create close button overlay")
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

        params.gravity = Gravity.TOP or Gravity.END
        params.x = 16
        params.y = 16
        
        closeButtonView = ComposeView(parentService).apply {
            setViewTreeLifecycleOwner(parentService.getLifecycleOwnerInstance())
            setViewTreeSavedStateRegistryOwner(parentService.getSavedStateRegistryOwnerInstance())
            setContent {
                LocalifyTheme {
                    // Close button
                    IconButton(
                        onClick = { hide() },
                        modifier = Modifier
                            .size(48.dp)
                            .background(
                                Color.Black.copy(alpha = 0.7f),
                                CircleShape
                            )
                    ) {
                        Icon(
                            imageVector = Icons.Default.Close,
                            contentDescription = "关闭",
                            tint = Color.White,
                            modifier = Modifier.size(24.dp)
                        )
                    }
                }
            }
        }
        windowManager.addView(closeButtonView, params)
    }

    
    @Composable
    private fun LeftControlSection() {
        Column(
            verticalArrangement = Arrangement.spacedBy(8.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // Top row: Q, W/↑, E
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                VirtualButton(
                    text = "Q",
                    keyCode = KEY_Q,
                    modifier = Modifier.size(48.dp)
                )
                if (isCenterButtonToggled) {
                    // Space button (↑)
                    VirtualButton(
                        text = "↑",
                        keyCode = KEY_SPACE,
                        modifier = Modifier.size(48.dp),
                        isSpecial = true
                    )
                } else {
                    VirtualButton(
                        text = "W",
                        keyCode = KEY_W,
                        modifier = Modifier.size(48.dp)
                    )
                }
                VirtualButton(
                    text = "E",
                    keyCode = KEY_E,
                    modifier = Modifier.size(48.dp)
                )
            }
            
            // Middle row: A/Hidden, Center, D/Hidden
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                if (isCenterButtonToggled) {
                    // Hidden A button - use invisible spacer
                    Spacer(modifier = Modifier.size(48.dp))
                } else {
                    VirtualButton(
                        text = "A",
                        keyCode = KEY_A,
                        modifier = Modifier.size(48.dp)
                    )
                }
                
                CenterToggleButton(
                    modifier = Modifier.size(48.dp)
                )
                
                if (isCenterButtonToggled) {
                    // Hidden D button - use invisible spacer
                    Spacer(modifier = Modifier.size(48.dp))
                } else {
                    VirtualButton(
                        text = "D",
                        keyCode = KEY_D,
                        modifier = Modifier.size(48.dp)
                    )
                }
            }
            
            // Bottom row: R, S/↓, F
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                VirtualButton(
                    text = "R",
                    keyCode = KEY_R,
                    modifier = Modifier.size(48.dp)
                )
                if (isCenterButtonToggled) {
                    // Control button (↓)
                    VirtualButton(
                        text = "↓",
                        keyCode = KEY_CTRL,
                        modifier = Modifier.size(48.dp),
                        isSpecial = true
                    )
                } else {
                    VirtualButton(
                        text = "S",
                        keyCode = KEY_S,
                        modifier = Modifier.size(48.dp)
                    )
                }
                VirtualButton(
                    text = "F",
                    keyCode = KEY_F,
                    modifier = Modifier.size(48.dp),
                    isSpecial = true
                )
            }
        }
    }
    
    @Composable
    private fun RightControlSection() {
        Column(
            verticalArrangement = Arrangement.spacedBy(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // Character switch buttons above virtual joystick
            CharacterSwitchButtons()
            
            // Virtual joystick below character switch buttons
            VirtualJoystick()
        }
    }
    
    @Composable
    private fun CenterToggleButton(modifier: Modifier = Modifier) {
        val coroutineScope = rememberCoroutineScope()
        var isPressed by remember { mutableStateOf(false) }
        var tapCount by remember { mutableIntStateOf(0) }
        
        Box(
            modifier = modifier
                .background(
                    if (isCenterButtonToggled) Color.Blue.copy(alpha = 0.6f) else Color.Gray.copy(alpha = 0.6f),
                    RoundedCornerShape(8.dp)
                )
                .border(
                    2.dp,
                    if (isCenterButtonToggled) Color.Blue else Color.White.copy(alpha = 0.5f),
                    RoundedCornerShape(8.dp)
                )
                .clickable(
                    interactionSource = remember { MutableInteractionSource() },
                    indication = null
                ) {
                    tapCount++
                    coroutineScope.launch {
                        delay(300) // Wait for potential double tap
                        if (tapCount == 1) {
                            // Single tap - no action
                        } else if (tapCount >= 2) {
                            // Double tap - toggle mode
                            isCenterButtonToggled = !isCenterButtonToggled
                        }
                        tapCount = 0
                    }
                },
            contentAlignment = Alignment.Center
        ) {
            if (isCenterButtonToggled) {
                // Show indicator that we're in space/control mode
                Column(
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center
                ) {
                    Text(
                        text = "↑",
                        color = Color.Gray,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold
                    )
                    Text(
                        text = "↓",
                        color = Color.Gray,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold
                    )
                }
            } else {
                Text(
                    text = "*",
                    color = Color.White,
                    fontSize = 20.sp,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center
                )
            }
        }
    }
    
    @Composable
    private fun VirtualButton(
        text: String,
        keyCode: Int,
        modifier: Modifier = Modifier,
        isSpecial: Boolean = false
    ) {
        val coroutineScope = rememberCoroutineScope()
        var isPressed by remember { mutableStateOf(false) }
        
        Box(
            modifier = modifier
                .background(
                    if (isSpecial) Color.Green.copy(alpha = 0.6f) else Color.Gray.copy(alpha = 0.6f),
                    RoundedCornerShape(8.dp)
                )
                .border(
                    2.dp,
                    if (isPressed) Color.Blue else Color.White.copy(alpha = 0.5f),
                    RoundedCornerShape(8.dp)
                )
                .pointerInput(keyCode) {
                    detectTapGestures(
                        onPress = {
                            Log.d(TAG, "Button pressed: text=$text, keyCode=$keyCode, isSpecial=$isSpecial")
                            isPressed = true
                            // All buttons now send key events
                            Log.d(TAG, "Starting key press for button: $text, keyCode=$keyCode")
                            startKeyPress(keyCode)
                            tryAwaitRelease()
                            Log.d(TAG, "Button released: $text, keyCode=$keyCode")
                            stopKeyPress(keyCode)
                            isPressed = false
                        }
                    )
                },
            contentAlignment = Alignment.Center
        ) {
            Text(
                text = text,
                color = Color.White,
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center
            )
        }
    }
    
    @Composable
    private fun VirtualJoystick() {
        val joystickSize = 120.dp
        val knobSize = 48.dp
        
        Box(
            modifier = Modifier.size(joystickSize),
            contentAlignment = Alignment.Center
        ) {
            // Joystick background
            Box(
                modifier = Modifier
                    .size(joystickSize)
                    .background(
                        Color.Gray.copy(alpha = 0.4f),
                        CircleShape
                    )
                    .border(2.dp, Color.White.copy(alpha = 0.5f), CircleShape)
            )
            
            // Joystick knob
            Box(
                modifier = Modifier
                    .size(knobSize)
                    .offset {
                        IntOffset(
                            x = joystickOffsetX.roundToInt(),
                            y = joystickOffsetY.roundToInt()
                        )
                    }
                    .background(Color.Blue.copy(alpha = 0.8f), CircleShape)
                    .border(2.dp, Color.White, CircleShape)
                    .pointerInput(Unit) {
                        detectDragGestures(
                            onDragStart = { offset ->
                                val center = size.center
                                joystickCenterX = center.x.toFloat()
                                joystickCenterY = center.y.toFloat()
                            },
                            onDragEnd = {
                                // Return to center
                                joystickOffsetX = 0f
                                joystickOffsetY = 0f
                                // Stop directional input
                                sendDirectionalInput(0f, 0f)
                            }
                        ) { change, _ ->
                            val maxRadius = (joystickSize.value - knobSize.value) / 2 * density
                            val deltaX = change.position.x - joystickCenterX
                            val deltaY = change.position.y - joystickCenterY
                            val distance = sqrt(deltaX * deltaX + deltaY * deltaY)
                            
                            if (distance <= maxRadius) {
                                joystickOffsetX = deltaX
                                joystickOffsetY = deltaY
                            } else {
                                val angle = atan2(deltaY, deltaX)
                                joystickOffsetX = cos(angle) * maxRadius
                                joystickOffsetY = sin(angle) * maxRadius
                            }
                            
                            // Convert to normalized values (-1 to 1)
                            val normalizedX = joystickOffsetX / maxRadius
                            val normalizedY = joystickOffsetY / maxRadius
                            sendDirectionalInput(normalizedX, normalizedY)
                        }
                    }
            )
        }
    }
    
    @Composable
    private fun CharacterSwitchButtons() {
        Column(
            verticalArrangement = Arrangement.spacedBy(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                VirtualButton(
                    text = "←",
                    keyCode = KEY_LEFT,
                    modifier = Modifier.size(56.dp)
                )
                VirtualButton(
                    text = "→",
                    keyCode = KEY_RIGHT,
                    modifier = Modifier.size(56.dp)
                )
            }
        }
    }
    
    
    private fun startKeyPress(keyCode: Int) {
        Log.d(TAG, "startKeyPress called: keyCode=$keyCode")
        if (activeKeys.contains(keyCode)) {
            Log.d(TAG, "Key already active, skipping: keyCode=$keyCode")
            return
        }
        
        activeKeys.add(keyCode)
        
        // Send initial key down event
        Log.d(TAG, "Sending initial key down event: keyCode=$keyCode")
        sendKeyEvent(keyCode, KeyEvent.ACTION_DOWN)
        
        // Start repeat task
        val repeatTask = object : Runnable {
            override fun run() {
                if (activeKeys.contains(keyCode)) {
                    sendKeyEvent(keyCode, KeyEvent.ACTION_DOWN)
                    handler.postDelayed(this, 50) // 20fps repeat rate
                }
            }
        }
        keyRepeatTasks[keyCode] = repeatTask
        handler.postDelayed(repeatTask, 100) // Initial delay before repeat
    }
    
    private fun stopKeyPress(keyCode: Int) {
        activeKeys.remove(keyCode)
        keyRepeatTasks[keyCode]?.let { task ->
            handler.removeCallbacks(task)
            keyRepeatTasks.remove(keyCode)
        }
        
        // Send key up event
        sendKeyEvent(keyCode, KeyEvent.ACTION_UP)
    }
    
    private fun stopAllKeyRepeats() {
        activeKeys.forEach { keyCode ->
            stopKeyPress(keyCode)
        }
        activeKeys.clear()
        keyRepeatTasks.clear()
    }
    
    private fun sendKeyEvent(keyCode: Int, action: Int) {
        try {
            Log.d(TAG, "Sending key event: keyCode=$keyCode, action=$action")
            
            // Create virtual keyboard input message
            val virtualKeyboardInput = VirtualKeyboardInput.newBuilder()
                .setKeyCode(keyCode)
                .setAction(action)
                .setRepeat(false)
                .build()
            
            // Send via AIDL service
            val aidlService = parentService.getAidlService()
            if (aidlService != null) {
                aidlService.sendMessage(MessageType.VIRTUAL_KEYBOARD_INPUT, virtualKeyboardInput)
            } else {
                Log.w(TAG, "AIDL service not available, cannot send key event")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending key event: keyCode=$keyCode, action=$action", e)
        }
    }
    
    private fun sendDirectionalInput(normalizedX: Float, normalizedY: Float) {
        try {
            Log.v(TAG, "Directional input: x=$normalizedX, y=$normalizedY")
            
            // Create virtual joystick input message
            val virtualJoystickInput = VirtualJoystickInput.newBuilder()
                .setAction(MotionEvent.ACTION_MOVE)
                .setLeftStickX(0f)
                .setLeftStickY(0f)
                .setRightStickX(normalizedX)
                .setRightStickY(normalizedY)
                .setLeftTrigger(0f)
                .setRightTrigger(0f)
                .setHatX(0f)
                .setHatY(0f)
                .build()
            
            // Send via AIDL service
            val aidlService = parentService.getAidlService()
            if (aidlService != null) {
                aidlService.sendMessage(MessageType.VIRTUAL_JOYSTICK_INPUT, virtualJoystickInput)
            } else {
                Log.w(TAG, "AIDL service not available, cannot send directional input")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending directional input", e)
        }
    }

    fun destroy() {
        hide()
    }
}