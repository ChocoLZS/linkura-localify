package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Gravity
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import io.github.chocolzs.linkura.localify.LinkuraHookMain
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import kotlin.math.*

class FreeCameraControlOverlayUI : BaseOverlay {
    override val overlayId = "free_camera_control"
    override val displayName = "Free Camera Control"

    override var onOverlayHidden: (() -> Unit)? = null
    override var onVisibilityChanged: ((Boolean) -> Unit)? = null

    companion object {
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

    private var isCreated = false
    private var isVisible = false
    private lateinit var windowManager: WindowManager

    // Three separate overlay views
    private lateinit var leftControlView: FrameLayout
    private lateinit var rightControlView: FrameLayout
    private lateinit var closeButtonView: FrameLayout

    // WASD center button state
    private var isCenterButtonToggled = false

    // Touch handling for dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f

    // Key press handling
    private val handler = Handler(Looper.getMainLooper())
    private val activeKeys = mutableSetOf<Int>()
    private val keyRepeatTasks = mutableMapOf<Int, Runnable>()

    // Virtual joystick state
    private var joystickCenterX = 0f
    private var joystickCenterY = 0f
    private var joystickOffsetX = 0f
    private var joystickOffsetY = 0f
    private var joystickMaxRadius = 0f

    // UI Components for center toggle button
    private lateinit var centerToggleButton: FrameLayout
    private lateinit var centerToggleText: TextView

    // UI Components for joystick
    private lateinit var joystickBackground: View
    private lateinit var joystickKnob: View

    @SuppressLint("ClickableViewAccessibility")
    override fun show(context: Context) {
        if (isVisible) return

        val activity = context as? Activity ?: return

        // Check if device is in landscape mode
        val isLandscape = activity.resources.configuration.orientation ==
            android.content.res.Configuration.ORIENTATION_LANDSCAPE

        if (!isLandscape) {
            Toast.makeText(
                activity,
                "请切换到横屏模式使用虚拟控制器",
                Toast.LENGTH_SHORT
            ).show()
            Log.d(TAG, "Free camera control overlay requires landscape mode")
            return
        }

        try {
            createLeftControlOverlay(activity)
            createRightControlOverlay(activity)
            createCloseButtonOverlay(activity)
            isVisible = true
            onVisibilityChanged?.invoke(true)
            Log.d(TAG, "Free camera control overlay shown")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing free camera control overlay", e)
        }
    }

    override fun hide() {
        if (!isVisible) return

        try {
            stopAllKeyRepeats()

            if (::leftControlView.isInitialized) {
                windowManager.removeView(leftControlView)
            }
            if (::rightControlView.isInitialized) {
                windowManager.removeView(rightControlView)
            }
            if (::closeButtonView.isInitialized) {
                windowManager.removeView(closeButtonView)
            }

            isVisible = false
            onVisibilityChanged?.invoke(false)
            onOverlayHidden?.invoke()
            Log.d(TAG, "Free camera control overlay hidden")
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding free camera control overlay", e)
        }
    }

    override fun isCreated(): Boolean = isCreated
    override fun isVisible(): Boolean = isVisible

    @SuppressLint("ClickableViewAccessibility")
    private fun createLeftControlOverlay(activity: Activity) {
        windowManager = activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        val density = activity.resources.displayMetrics.density

        // Create main container
        leftControlView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        // Create content container
        val contentContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )

            val padding = (8f * density).toInt()
            setPadding(padding, padding, padding, padding)
        }

        // Create top row: Q, W/↑, E
        val topRow = createButtonRow(activity, listOf(
            ButtonData("Q", KEY_Q),
            ButtonData("W", KEY_W, "↑", KEY_SPACE),
            ButtonData("E", KEY_E)
        ))
        contentContainer.addView(topRow)

        // Create middle row: A/Hidden, Center, D/Hidden
        val middleRow = createMiddleRow(activity)
        contentContainer.addView(middleRow)

        // Create bottom row: R, S/↓, F
        val bottomRow = createButtonRow(activity, listOf(
            ButtonData("R", KEY_R),
            ButtonData("S", KEY_S, "↓", KEY_CTRL),
            ButtonData("F", KEY_F, isSpecial = true)
        ))
        contentContainer.addView(bottomRow)

        leftControlView.addView(contentContainer)

        // Add to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.WRAP_CONTENT
            height = WindowManager.LayoutParams.WRAP_CONTENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.BOTTOM or Gravity.START
            x = (32 * density).toInt()
            y = (32 * density).toInt()
        }

        try {
            windowManager.addView(leftControlView, layoutParams)
            isCreated = true
            Log.d(TAG, "Left control overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create left control overlay", e)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createRightControlOverlay(activity: Activity) {
        val density = activity.resources.displayMetrics.density

        // Create main container
        rightControlView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        // Create content container
        val contentContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )

            val padding = (8f * density).toInt()
            setPadding(padding, padding, padding, padding)
        }

        // Character switch buttons
        val characterSwitchContainer = createCharacterSwitchButtons(activity)
        contentContainer.addView(characterSwitchContainer)

        // Add spacing
        val spacer = View(activity).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                (16f * density).toInt()
            )
        }
        contentContainer.addView(spacer)

        // Virtual joystick
        val joystickContainer = createVirtualJoystick(activity)
        contentContainer.addView(joystickContainer)

        rightControlView.addView(contentContainer)

        // Add to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.WRAP_CONTENT
            height = WindowManager.LayoutParams.WRAP_CONTENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.BOTTOM or Gravity.END
            x = (32 * density).toInt()
            y = (32 * density).toInt()
        }

        try {
            windowManager.addView(rightControlView, layoutParams)
            Log.d(TAG, "Right control overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create right control overlay", e)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createCloseButtonOverlay(activity: Activity) {
        val density = activity.resources.displayMetrics.density

        // Create main container
        closeButtonView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        // Create close button
        val closeButton = ImageButton(activity).apply {
            val size = (48f * density).toInt()
            layoutParams = FrameLayout.LayoutParams(size, size)

            val cornerRadius = 24f * density
            background = createRoundedBackground(Color.parseColor("#B3000000"), cornerRadius)
            setImageDrawable(SVGIcon.Close.createDrawable(activity, Color.WHITE, 24f))
            scaleType = ImageView.ScaleType.CENTER

            setOnClickListener {
                hide()
            }
        }

        closeButtonView.addView(closeButton)

        // Add to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.WRAP_CONTENT
            height = WindowManager.LayoutParams.WRAP_CONTENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.TOP or Gravity.END
            x = (16 * density).toInt()
            y = (16 * density).toInt()
        }

        try {
            windowManager.addView(closeButtonView, layoutParams)
            Log.d(TAG, "Close button overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create close button overlay", e)
        }
    }

    private data class ButtonData(
        val normalText: String,
        val normalKeyCode: Int,
        val toggledText: String? = null,
        val toggledKeyCode: Int? = null,
        val isSpecial: Boolean = false
    )

    @SuppressLint("ClickableViewAccessibility")
    private fun createButtonRow(activity: Activity, buttons: List<ButtonData>): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (8f * density).toInt()
            }

            buttons.forEach { buttonData ->
                val button = createVirtualButton(activity, buttonData)
                addView(button)

                // Add spacing between buttons
                if (buttonData != buttons.last()) {
                    val spacer = View(activity).apply {
                        layoutParams = LinearLayout.LayoutParams(
                            (8f * density).toInt(),
                            0
                        )
                    }
                    addView(spacer)
                }
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createMiddleRow(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (8f * density).toInt()
            }

            // A button or spacer
            val leftButton = if (isCenterButtonToggled) {
                createSpacer(activity, (48f * density).toInt())
            } else {
                createVirtualButton(activity, ButtonData("A", KEY_A))
            }
            addView(leftButton)

            // Spacing
            addView(createSpacer(activity, (8f * density).toInt()))

            // Center toggle button
            centerToggleButton = createCenterToggleButton(activity)
            addView(centerToggleButton)

            // Spacing
            addView(createSpacer(activity, (8f * density).toInt()))

            // D button or spacer
            val rightButton = if (isCenterButtonToggled) {
                createSpacer(activity, (48f * density).toInt())
            } else {
                createVirtualButton(activity, ButtonData("D", KEY_D))
            }
            addView(rightButton)
        }
    }

    private fun createSpacer(activity: Activity, size: Int): View {
        return View(activity).apply {
            layoutParams = LinearLayout.LayoutParams(size, size)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createCenterToggleButton(activity: Activity): FrameLayout {
        val density = activity.resources.displayMetrics.density
        var tapCount = 0
        var lastTapTime = 0L

        return FrameLayout(activity).apply {
            val size = (48f * density).toInt()
            layoutParams = LinearLayout.LayoutParams(size, size)

            val cornerRadius = 8f * density
            background = createRoundedBackground(
                if (isCenterButtonToggled) Color.parseColor("#99007ACC") else Color.parseColor("#99999999"),
                cornerRadius
            )

            centerToggleText = TextView(activity).apply {
                layoutParams = FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.WRAP_CONTENT,
                    FrameLayout.LayoutParams.WRAP_CONTENT,
                    Gravity.CENTER
                )
                text = if (isCenterButtonToggled) "↑\n↓" else "*"
                setTextColor(Color.WHITE)
                textSize = if (isCenterButtonToggled) 12f else 20f
                gravity = Gravity.CENTER
            }
            addView(centerToggleText)

            setOnClickListener {
                val currentTime = System.currentTimeMillis()
                if (currentTime - lastTapTime < 300) {
                    tapCount++
                } else {
                    tapCount = 1
                }
                lastTapTime = currentTime

                handler.postDelayed({
                    if (tapCount >= 2) {
                        // Double tap - toggle mode
                        isCenterButtonToggled = !isCenterButtonToggled
                        updateCenterToggleButton()
                        updateMiddleRowButtons()
                        updateButtonTexts()
                    }
                    tapCount = 0
                }, 300)
            }
        }
    }

    private fun updateCenterToggleButton() {
        if (::centerToggleButton.isInitialized) {
            val density = centerToggleButton.context.resources.displayMetrics.density
            val cornerRadius = 8f * density

            centerToggleButton.background = createRoundedBackground(
                if (isCenterButtonToggled) Color.parseColor("#99007ACC") else Color.parseColor("#99999999"),
                cornerRadius
            )

            centerToggleText.text = if (isCenterButtonToggled) "↑\n↓" else "*"
            centerToggleText.textSize = if (isCenterButtonToggled) 12f else 20f
        }
    }

    private fun updateMiddleRowButtons() {
        // This would require recreating the middle row, which is complex
        // For now, we'll keep the buttons but they won't be functional when toggled
        Log.d(TAG, "Center button toggled: $isCenterButtonToggled")
    }

    private fun updateButtonTexts() {
        // Update button texts based on toggle state
        // This is simplified - in a full implementation, we'd track all button references
        Log.d(TAG, "Updating button texts for toggle state: $isCenterButtonToggled")
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createVirtualButton(activity: Activity, buttonData: ButtonData): FrameLayout {
        val density = activity.resources.displayMetrics.density
        var isPressed = false

        return FrameLayout(activity).apply {
            val size = (48f * density).toInt()
            layoutParams = LinearLayout.LayoutParams(size, size)

            val cornerRadius = 8f * density
            background = createRoundedBackground(
                if (buttonData.isSpecial) Color.parseColor("#9900CC00") else Color.parseColor("#99999999"),
                cornerRadius
            )

            val textView = TextView(activity).apply {
                layoutParams = FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.WRAP_CONTENT,
                    FrameLayout.LayoutParams.WRAP_CONTENT,
                    Gravity.CENTER
                )

                // Determine which text and key code to use
                val displayText = if (isCenterButtonToggled && buttonData.toggledText != null) {
                    buttonData.toggledText
                } else {
                    buttonData.normalText
                }

                text = displayText
                setTextColor(Color.WHITE)
                textSize = 16f
                gravity = Gravity.CENTER
            }
            addView(textView)

            setOnTouchListener { _, event ->
                when (event.action) {
                    MotionEvent.ACTION_DOWN -> {
                        isPressed = true
                        background = createRoundedBackground(
                            Color.parseColor("#99007ACC"),
                            cornerRadius
                        )

                        // Determine which key code to send
                        val keyCode = if (isCenterButtonToggled && buttonData.toggledKeyCode != null) {
                            buttonData.toggledKeyCode
                        } else {
                            buttonData.normalKeyCode
                        }

                        Log.d(TAG, "Button pressed: ${buttonData.normalText}, keyCode=$keyCode")
                        startKeyPress(keyCode)
                        true
                    }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                        if (isPressed) {
                            isPressed = false
                            background = createRoundedBackground(
                                if (buttonData.isSpecial) Color.parseColor("#9900CC00") else Color.parseColor("#99999999"),
                                cornerRadius
                            )

                            // Determine which key code to send
                            val keyCode = if (isCenterButtonToggled && buttonData.toggledKeyCode != null) {
                                buttonData.toggledKeyCode
                            } else {
                                buttonData.normalKeyCode
                            }

                            Log.d(TAG, "Button released: ${buttonData.normalText}, keyCode=$keyCode")
                            stopKeyPress(keyCode)
                        }
                        true
                    }
                    else -> false
                }
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createCharacterSwitchButtons(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            gravity = Gravity.CENTER

            val leftButton = createVirtualButton(activity, ButtonData("←", KEY_LEFT))
            leftButton.layoutParams = LinearLayout.LayoutParams(
                (56f * density).toInt(),
                (56f * density).toInt()
            )
            addView(leftButton)

            // Spacing
            val spacer = View(activity).apply {
                layoutParams = LinearLayout.LayoutParams(
                    (16f * density).toInt(),
                    0
                )
            }
            addView(spacer)

            val rightButton = createVirtualButton(activity, ButtonData("→", KEY_RIGHT))
            rightButton.layoutParams = LinearLayout.LayoutParams(
                (56f * density).toInt(),
                (56f * density).toInt()
            )
            addView(rightButton)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createVirtualJoystick(activity: Activity): FrameLayout {
        val density = activity.resources.displayMetrics.density
        val joystickSize = (120f * density).toInt()
        val knobSize = (48f * density).toInt()

        return FrameLayout(activity).apply {
            layoutParams = LinearLayout.LayoutParams(joystickSize, joystickSize)

            // Joystick background
            joystickBackground = View(activity).apply {
                layoutParams = FrameLayout.LayoutParams(joystickSize, joystickSize)
                val cornerRadius = joystickSize / 2f
                background = createRoundedBackground(Color.parseColor("#66999999"), cornerRadius)
            }
            addView(joystickBackground)

            // Joystick knob
            joystickKnob = View(activity).apply {
                layoutParams = FrameLayout.LayoutParams(knobSize, knobSize, Gravity.CENTER)
                val cornerRadius = knobSize / 2f
                background = createRoundedBackground(Color.parseColor("#CC007ACC"), cornerRadius)
            }
            addView(joystickKnob)

            joystickMaxRadius = (joystickSize - knobSize) / 2f

            // Initialize center coordinates
            post {
                joystickCenterX = width / 2f
                joystickCenterY = height / 2f
            }

            setOnTouchListener { _, event ->
                when (event.action) {
                    MotionEvent.ACTION_DOWN -> {
                        // Ensure center coordinates are set
                        if (joystickCenterX == 0f || joystickCenterY == 0f) {
                            joystickCenterX = width / 2f
                            joystickCenterY = height / 2f
                        }
                        true
                    }
                    MotionEvent.ACTION_MOVE -> {
                        val deltaX = event.x - joystickCenterX
                        val deltaY = event.y - joystickCenterY
                        val distance = sqrt(deltaX * deltaX + deltaY * deltaY)

                        if (distance <= joystickMaxRadius) {
                            joystickOffsetX = deltaX
                            joystickOffsetY = deltaY
                        } else {
                            val angle = atan2(deltaY, deltaX)
                            joystickOffsetX = cos(angle) * joystickMaxRadius
                            joystickOffsetY = sin(angle) * joystickMaxRadius
                        }

                        // Update knob position using translation instead of margins
                        joystickKnob.translationX = joystickOffsetX
                        joystickKnob.translationY = joystickOffsetY

                        // Convert to normalized values (-1 to 1)
                        val normalizedX = joystickOffsetX / joystickMaxRadius
                        val normalizedY = joystickOffsetY / joystickMaxRadius
                        sendDirectionalInput(normalizedX, normalizedY)
                        true
                    }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                        // Return to center
                        joystickOffsetX = 0f
                        joystickOffsetY = 0f

                        // Reset knob position using translation
                        joystickKnob.translationX = 0f
                        joystickKnob.translationY = 0f

                        sendDirectionalInput(0f, 0f)
                        true
                    }
                    else -> false
                }
            }
        }
    }

    private fun createRoundedBackground(color: Int, cornerRadius: Float): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            setColor(color)
            setCornerRadius(cornerRadius)
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

            // Send via native hook
            LinkuraHookMain.keyboardEvent(keyCode, action)
        } catch (e: Exception) {
            Log.e(TAG, "Error sending key event: keyCode=$keyCode, action=$action", e)
        }
    }

    private fun sendDirectionalInput(normalizedX: Float, normalizedY: Float) {
        try {
            Log.v(TAG, "Directional input: x=$normalizedX, y=$normalizedY")

            // Send via native hook - right stick for camera control
            LinkuraHookMain.joystickEvent(
                action = MotionEvent.ACTION_MOVE,
                leftStickX = 0f,
                leftStickY = 0f,
                rightStickX = normalizedX,
                rightStickY = normalizedY,
                leftTrigger = 0f,
                rightTrigger = 0f,
                hatX = 0f,
                hatY = 0f
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error sending directional input", e)
        }
    }

    override fun destroy() {
        hide()
        super.destroy()
    }
}