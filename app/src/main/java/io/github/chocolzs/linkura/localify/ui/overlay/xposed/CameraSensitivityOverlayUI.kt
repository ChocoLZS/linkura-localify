package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.ImageButton
import android.widget.LinearLayout
import android.widget.TextView
import io.github.chocolzs.linkura.localify.LinkuraHookMain
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*

class CameraSensitivityOverlayUI : BaseOverlay {
    override val overlayId = "camera_sensitivity"
    override val displayName = "Camera Sensitivity"

    override var onOverlayHidden: (() -> Unit)? = null
    override var onVisibilityChanged: ((Boolean) -> Unit)? = null

    private var isCreated = false
    private var isVisible = false
    private lateinit var windowManager: WindowManager
    private lateinit var overlayView: FrameLayout
    private lateinit var contentContainer: LinearLayout

    // UI Components
    private lateinit var titleText: TextView
    private lateinit var closeButton: ImageButton
    private lateinit var resetButton: Button
    private lateinit var controlsContainer: LinearLayout

    // Sensitivity controls
    private lateinit var movementSensitivityControl: SensitivityControlView
    private lateinit var verticalSensitivityControl: SensitivityControlView
    private lateinit var fovSensitivityControl: SensitivityControlView
    private lateinit var rotationSensitivityControl: SensitivityControlView

    // Sensitivity values
    private var movementSensitivity = 1.0f
    private var verticalSensitivity = 1.0f
    private var fovSensitivity = 1.0f
    private var rotationSensitivity = 1.0f

    // Touch handling for window dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f

    // Handler for UI updates
    private val handler = Handler(Looper.getMainLooper())

    @SuppressLint("ClickableViewAccessibility")
    override fun show(context: Context) {
        if (isVisible) return

        val activity = context as? Activity ?: return

        try {
            loadCurrentSensitivityValues()
            createOverlay(activity)
            isVisible = true
            onVisibilityChanged?.invoke(true)
            Log.d(TAG, "Xposed camera sensitivity overlay shown")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing Xposed camera sensitivity overlay", e)
        }
    }

    override fun hide() {
        if (!isVisible) return

        try {
            if (::overlayView.isInitialized) {
                windowManager.removeView(overlayView)
            }
            isVisible = false
            onVisibilityChanged?.invoke(false)
            onOverlayHidden?.invoke()
            Log.d(TAG, "Xposed camera sensitivity overlay hidden")
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding Xposed camera sensitivity overlay", e)
        }
    }

    override fun isCreated(): Boolean = isCreated
    override fun isVisible(): Boolean = isVisible

    private fun loadCurrentSensitivityValues() {
        // TODO: Load from preferences or native layer
        // For now, use default values
        movementSensitivity = 1.0f
        verticalSensitivity = 1.0f
        fovSensitivity = 1.0f
        rotationSensitivity = 1.0f
        Log.d(TAG, "Loaded sensitivity values: movement=$movementSensitivity, vertical=$verticalSensitivity, fov=$fovSensitivity, rotation=$rotationSensitivity")
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createOverlay(activity: Activity) {
        windowManager = activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager

        val density = activity.resources.displayMetrics.density
        val isLandscape = activity.resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE

        // Create main overlay container
        overlayView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        // Create content container with adaptive width
        val containerWidth = if (isLandscape) (360f * density).toInt() else (280f * density).toInt()
        contentContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                containerWidth,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )

            val cornerRadius = 12f * density
            background = createRoundedBackground(Color.parseColor("#CC000000"), cornerRadius)

            val padding = (16f * density).toInt()
            setPadding(padding, padding, padding, padding)
        }

        createUI(activity, isLandscape)
        overlayView.addView(contentContainer)

        // Set up touch handling for window dragging
        setupTouchHandling(activity)

        // Add overlay to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.WRAP_CONTENT
            height = WindowManager.LayoutParams.WRAP_CONTENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.CENTER
            x = 0
            y = 0
        }

        try {
            windowManager.addView(overlayView, layoutParams)
            isCreated = true
            Log.d(TAG, "Xposed camera sensitivity overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create Xposed camera sensitivity overlay", e)
            isCreated = false
        }
    }

    private fun createUI(activity: Activity, isLandscape: Boolean) {
        val density = activity.resources.displayMetrics.density

        // Title bar
        val titleBar = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (8f * density).toInt()
            }
        }

        // Title
        titleText = TextView(activity).apply {
            text = "Camera Sensitivity"
            setTextColor(Color.WHITE)
            textSize = 16f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1f
            )
        }
        titleBar.addView(titleText)

        // Reset button
        resetButton = Button(activity).apply {
            text = "↻"
            setTextColor(Color.WHITE)
            textSize = 14f
            layoutParams = LinearLayout.LayoutParams(
                (32f * density).toInt(),
                (32f * density).toInt()
            ).apply {
                rightMargin = (4f * density).toInt()
            }
            background = createRoundedBackground(Color.parseColor("#66FFFFFF"), 4f * density)
            setOnClickListener { resetAllToDefault() }
        }
        titleBar.addView(resetButton)

        // Close button
        closeButton = ImageButton(activity).apply {
            val size = (32f * density).toInt()
            layoutParams = LinearLayout.LayoutParams(size, size)
            background = createRoundedBackground(Color.parseColor("#66FFFFFF"), 4f * density)
            setImageDrawable(SVGIcon.Close.createDrawable(activity, Color.WHITE, 14f))
            scaleType = android.widget.ImageView.ScaleType.CENTER
            setOnClickListener { hide() }
        }
        titleBar.addView(closeButton)

        contentContainer.addView(titleBar)

        // Divider
        val divider = View(activity).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                (1f * density).toInt()
            ).apply {
                bottomMargin = (8f * density).toInt()
            }
            setBackgroundColor(Color.GRAY)
        }
        contentContainer.addView(divider)

        // Controls container
        controlsContainer = LinearLayout(activity).apply {
            orientation = if (isLandscape) LinearLayout.VERTICAL else LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }

        createSensitivityControls(activity, isLandscape)
        contentContainer.addView(controlsContainer)
    }

    private fun createSensitivityControls(activity: Activity, isLandscape: Boolean) {
        val density = activity.resources.displayMetrics.density

        if (isLandscape) {
            // Landscape: 2x2 grid layout
            val row1 = LinearLayout(activity).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    bottomMargin = (8f * density).toInt()
                }
            }

            movementSensitivityControl = SensitivityControlView(activity, "Movement", movementSensitivity) { newValue ->
                movementSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply {
                    rightMargin = (6f * density).toInt()
                }
            }

            verticalSensitivityControl = SensitivityControlView(activity, "Vertical", verticalSensitivity) { newValue ->
                verticalSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply {
                    leftMargin = (6f * density).toInt()
                }
            }

            row1.addView(movementSensitivityControl)
            row1.addView(verticalSensitivityControl)
            controlsContainer.addView(row1)

            val row2 = LinearLayout(activity).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )
            }

            fovSensitivityControl = SensitivityControlView(activity, "FOV", fovSensitivity) { newValue ->
                fovSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply {
                    rightMargin = (6f * density).toInt()
                }
            }

            rotationSensitivityControl = SensitivityControlView(activity, "Rotation", rotationSensitivity) { newValue ->
                rotationSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f).apply {
                    leftMargin = (6f * density).toInt()
                }
            }

            row2.addView(fovSensitivityControl)
            row2.addView(rotationSensitivityControl)
            controlsContainer.addView(row2)
        } else {
            // Portrait: 1x4 column layout
            val spacing = (8f * density).toInt()

            movementSensitivityControl = SensitivityControlView(activity, "Movement", movementSensitivity) { newValue ->
                movementSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { bottomMargin = spacing }
            }

            verticalSensitivityControl = SensitivityControlView(activity, "Vertical", verticalSensitivity) { newValue ->
                verticalSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { bottomMargin = spacing }
            }

            fovSensitivityControl = SensitivityControlView(activity, "FOV", fovSensitivity) { newValue ->
                fovSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply { bottomMargin = spacing }
            }

            rotationSensitivityControl = SensitivityControlView(activity, "Rotation", rotationSensitivity) { newValue ->
                rotationSensitivity = newValue
                sendSensitivityUpdateToNative()
            }.apply {
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )
            }

            controlsContainer.addView(movementSensitivityControl)
            controlsContainer.addView(verticalSensitivityControl)
            controlsContainer.addView(fovSensitivityControl)
            controlsContainer.addView(rotationSensitivityControl)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setupTouchHandling(activity: Activity) {
        overlayView.setOnTouchListener { view, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    val layoutParams = overlayView.layoutParams as WindowManager.LayoutParams
                    initialX = layoutParams.x
                    initialY = layoutParams.y
                    initialTouchX = event.rawX
                    initialTouchY = event.rawY
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    val layoutParams = overlayView.layoutParams as WindowManager.LayoutParams
                    layoutParams.x = initialX + (event.rawX - initialTouchX).toInt()
                    layoutParams.y = initialY + (event.rawY - initialTouchY).toInt()
                    windowManager.updateViewLayout(overlayView, layoutParams)
                    true
                }
                else -> false
            }
        }
    }

    private fun resetAllToDefault() {
        movementSensitivity = 1.0f
        verticalSensitivity = 1.0f
        fovSensitivity = 1.0f
        rotationSensitivity = 1.0f

        // Update UI
        if (::movementSensitivityControl.isInitialized) {
            movementSensitivityControl.updateValue(movementSensitivity)
            verticalSensitivityControl.updateValue(verticalSensitivity)
            fovSensitivityControl.updateValue(fovSensitivity)
            rotationSensitivityControl.updateValue(rotationSensitivity)
        }

        sendSensitivityUpdateToNative()
        Log.d(TAG, "Reset all sensitivity values to default")
    }

    private fun sendSensitivityUpdateToNative() {
        try {
            // Create config update message
            val configUpdate = ConfigUpdate.newBuilder()
                .setUpdateType(ConfigUpdateType.FULL_UPDATE)
                .setCameraMovementSensitivity(movementSensitivity)
                .setCameraVerticalSensitivity(verticalSensitivity)
                .setCameraFovSensitivity(fovSensitivity)
                .setCameraRotationSensitivity(rotationSensitivity)
                .build()

            // Send via LinkuraHookMain (assuming there's a method to send config updates)
            // For now, we'll call the native updateConfig method directly
            LinkuraHookMain.updateConfig(configUpdate.toByteArray())

            Log.d(TAG, "Sensitivity update sent: movement=$movementSensitivity, vertical=$verticalSensitivity, fov=$fovSensitivity, rotation=$rotationSensitivity")
        } catch (e: Exception) {
            Log.e(TAG, "Error sending sensitivity update", e)
        }
    }

    private fun createRoundedBackground(color: Int, cornerRadius: Float): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            setColor(color)
            setCornerRadius(cornerRadius)
        }
    }

    // Custom sensitivity control view
    private inner class SensitivityControlView(
        activity: Activity,
        label: String,
        initialValue: Float,
        private val onValueChange: (Float) -> Unit
    ) : LinearLayout(activity) {

        private val labelText: TextView
        private val decreaseButton: Button
        private val valueText: TextView
        private val increaseButton: Button
        private var currentValue: Float = initialValue

        private val step = 0.01f
        private val minValue = 0.1f
        private val maxValue = 5.0f

        // Long press handling
        private var isLongPressing = false
        private val longPressHandler = Handler(Looper.getMainLooper())
        private var longPressRunnable: Runnable? = null

        init {
            orientation = VERTICAL
            val density = activity.resources.displayMetrics.density

            // Label
            labelText = TextView(activity).apply {
                text = label
                setTextColor(Color.WHITE)
                textSize = 12f
                layoutParams = LayoutParams(
                    LayoutParams.MATCH_PARENT,
                    LayoutParams.WRAP_CONTENT
                ).apply {
                    bottomMargin = (4f * density).toInt()
                }
            }
            addView(labelText)

            // Controls row
            val controlsRow = LinearLayout(activity).apply {
                orientation = HORIZONTAL
                layoutParams = LayoutParams(
                    LayoutParams.MATCH_PARENT,
                    LayoutParams.WRAP_CONTENT
                )
            }

            // Decrease button
            decreaseButton = Button(activity).apply {
                text = "−"
                setTextColor(Color.WHITE)
                textSize = 14f
                typeface = android.graphics.Typeface.DEFAULT_BOLD
                val size = (36f * density).toInt()
                layoutParams = LayoutParams(size, size).apply {
                    rightMargin = (6f * density).toInt()
                }
                background = createRoundedBackground(Color.parseColor("#CC666666"), 4f * density)
                setOnClickListener { decreaseValue() }
                setOnLongClickListener { startLongPress(false); true }
            }

            // Value display
            valueText = TextView(activity).apply {
                text = String.format("%.2f", currentValue)
                setTextColor(Color.WHITE)
                textSize = 16f
                typeface = android.graphics.Typeface.DEFAULT_BOLD
                gravity = Gravity.CENTER
                layoutParams = LayoutParams(
                    0,
                    (36f * density).toInt(),
                    1f
                )
            }

            // Increase button
            increaseButton = Button(activity).apply {
                text = "+"
                setTextColor(Color.WHITE)
                textSize = 14f
                typeface = android.graphics.Typeface.DEFAULT_BOLD
                val size = (36f * density).toInt()
                layoutParams = LayoutParams(size, size).apply {
                    leftMargin = (6f * density).toInt()
                }
                background = createRoundedBackground(Color.parseColor("#CC666666"), 4f * density)
                setOnClickListener { increaseValue() }
                setOnLongClickListener { startLongPress(true); true }
            }

            controlsRow.addView(decreaseButton)
            controlsRow.addView(valueText)
            controlsRow.addView(increaseButton)
            addView(controlsRow)

            // Set up touch listeners to stop long press
            setOnTouchListener { _, event ->
                if (event.action == MotionEvent.ACTION_UP || event.action == MotionEvent.ACTION_CANCEL) {
                    stopLongPress()
                }
                false
            }
        }

        private fun decreaseValue() {
            val newValue = (currentValue - step).coerceIn(minValue, maxValue)
            updateValue(newValue)
            onValueChange(newValue)
        }

        private fun increaseValue() {
            val newValue = (currentValue + step).coerceIn(minValue, maxValue)
            updateValue(newValue)
            onValueChange(newValue)
        }

        fun updateValue(newValue: Float) {
            currentValue = newValue
            valueText.text = String.format("%.2f", currentValue)
        }

        private fun startLongPress(increase: Boolean) {
            isLongPressing = true
            longPressRunnable = object : Runnable {
                override fun run() {
                    if (isLongPressing) {
                        if (increase) increaseValue() else decreaseValue()
                        longPressHandler.postDelayed(this, 100)
                    }
                }
            }
            longPressHandler.postDelayed(longPressRunnable!!, 500) // Initial delay
        }

        private fun stopLongPress() {
            isLongPressing = false
            longPressRunnable?.let { longPressHandler.removeCallbacks(it) }
        }
    }
}