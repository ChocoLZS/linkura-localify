package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.ImageButton
import android.widget.ImageView
import android.widget.LinearLayout
import android.graphics.drawable.GradientDrawable
import io.github.chocolzs.linkura.localify.LinkuraHookMain
import io.github.chocolzs.linkura.localify.TAG

class OverlayToolbarUI {
    private var isCreated = false
    private var isExpanded = true // 默认展开状态
    private var isCameraMenuVisible = false
    private var gameActivity: Activity? = null
    private val overlayManager = OverlayManager()
    private lateinit var windowManager: WindowManager
    private lateinit var overlayView: FrameLayout
    private lateinit var toolbarContainer: LinearLayout
    private lateinit var dragHandle: ImageView
    private lateinit var closeButton: ImageButton
    private lateinit var mainToolbar: LinearLayout
    private lateinit var cameraSecondaryMenu: LinearLayout
    private lateinit var archiveButton: ImageButton
    private lateinit var cameraInfoButton: ImageButton
    private lateinit var cameraSensitivityButton: ImageButton
    private lateinit var freeCameraControlButton: ImageButton
    private lateinit var colorPickerButton: FrameLayout
    private lateinit var colorIndicator: View

    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f

    @SuppressLint("ClickableViewAccessibility")
    fun createOverlay(context: Context) {
        if (isCreated) return
        isCreated = true

        val activity = context as? Activity ?: return
        gameActivity = activity

        // Setup overlay manager
        overlayManager.setGameActivity(activity)
        setupOverlays()

        // Ensure we're on the main thread
        activity.runOnUiThread {
            try {
                createOverlayInternal(activity)
            } catch (e: Exception) {
                Log.e(TAG, "Failed to create overlay toolbar", e)
                isCreated = false
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createOverlayInternal(activity: Activity) {
        windowManager = activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager

        // Create the main overlay container
        overlayView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        // Create main toolbar container
        toolbarContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )
            setBackgroundColor(Color.TRANSPARENT)
            val containerPadding = (4f * activity.resources.displayMetrics.density).toInt()
            setPadding(containerPadding, containerPadding, containerPadding, containerPadding)
            gravity = Gravity.CENTER_HORIZONTAL
        }

        // Create all UI components
        createControlBar(activity)
        createMainToolbar(activity)
        createCameraSecondaryMenu(activity)

        // Add components to container
        toolbarContainer.addView(createControlBarContainer(activity))
        toolbarContainer.addView(createMainToolbarContainer(activity))
        toolbarContainer.addView(cameraSecondaryMenu)

        overlayView.addView(toolbarContainer)

        // Set up touch handling for dragging
        setupTouchHandling()

        // Update initial visibility
        updateToolbarVisibility()

        // Add overlay to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.WRAP_CONTENT
            height = WindowManager.LayoutParams.WRAP_CONTENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.TOP or Gravity.CENTER_HORIZONTAL
            x = 0
            y = 100
        }

        try {
            windowManager.addView(overlayView, layoutParams)
            Log.d(TAG, "Overlay toolbar created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create overlay toolbar", e)
            isCreated = false
        }
    }

    private fun createControlBar(activity: Activity) {
        // Create drag handle
        dragHandle = ImageView(activity).apply {
            layoutParams = LinearLayout.LayoutParams(
                (24f * activity.resources.displayMetrics.density).toInt(),
                (24f * activity.resources.displayMetrics.density).toInt()
            )
            setImageDrawable(SVGIcon.DragIndicator.createDrawable(activity, Color.argb(179, 255, 255, 255), 14f))
            scaleType = ImageView.ScaleType.CENTER
        }

        // Create close button
        closeButton = ImageButton(activity).apply {
            val size = (24f * activity.resources.displayMetrics.density).toInt()
            layoutParams = LinearLayout.LayoutParams(size, size)
            setBackgroundColor(Color.TRANSPARENT)
            setImageDrawable(SVGIcon.Close.createDrawable(activity, Color.WHITE, 14f))
            scaleType = ImageView.ScaleType.CENTER

            setOnClickListener {
                Log.d(TAG, "Close button clicked - stopping overlay service")

                // Hide all overlays first
                overlayManager.hideAllOverlays()

                // Remove the toolbar itself
                removeOverlay()
            }
        }
    }

    private fun createControlBarContainer(activity: Activity): LinearLayout {
        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (8f * activity.resources.displayMetrics.density).toInt()
            }
            setBackgroundColor(Color.parseColor("#CC000000"))

            val cornerRadius = 12f * activity.resources.displayMetrics.density
            background = createRoundedBackground(Color.parseColor("#CC000000"), cornerRadius)

            val padding = (4f * activity.resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
            gravity = Gravity.CENTER_VERTICAL or Gravity.CENTER_HORIZONTAL

            // Add components with spacing
            addView(dragHandle)

            // Add spacer
            val spacer = View(activity).apply {
                layoutParams = LinearLayout.LayoutParams(
                    (4f * activity.resources.displayMetrics.density).toInt(),
                    0
                )
            }
            addView(spacer)
            addView(closeButton)
        }
    }

    private fun createMainToolbar(activity: Activity) {
        mainToolbar = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            gravity = Gravity.CENTER_VERTICAL
        }
    }

    private fun createMainToolbarContainer(activity: Activity): LinearLayout {
        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (8f * activity.resources.displayMetrics.density).toInt()
            }

            val cornerRadius = 20f * activity.resources.displayMetrics.density
            background = createRoundedBackground(Color.parseColor("#CC000000"), cornerRadius)

            val padding = (12f * activity.resources.displayMetrics.density).toInt()
            val verticalPadding = (8f * activity.resources.displayMetrics.density).toInt()
            setPadding(padding, verticalPadding, padding, verticalPadding)

            // Camera button
            addView(createMainToolButton(activity, SVGIcon.PhotoCamera::createDrawable, "Camera") {
                toggleCameraMenu()
            })

            // Archive button
            archiveButton = createMainToolButton(activity, SVGIcon.PlayArrow::createDrawable, "Archive") {
                Log.d(TAG, "Archive button clicked")
                // Let overlay manager handle the toggle
            }
            addView(archiveButton)

            // Bind archive button to overlay manager
            overlayManager.bindButton(archiveButton, "archive") { button, isSelected ->
                updateArchiveButtonVisual(button, isSelected)
            }

            // Bind camera info button to overlay manager
            overlayManager.bindButton(cameraInfoButton, "camera_info") { button, isSelected ->
                updateCameraInfoButtonVisual(button, isSelected)
                // Close camera menu when overlay is hidden (isSelected = false)
                if (!isSelected) {
                    isCameraMenuVisible = false
                    updateToolbarVisibility()
                }
            }

            // Bind camera sensitivity button to overlay manager
            overlayManager.bindButton(cameraSensitivityButton, "camera_sensitivity") { button, isSelected ->
                updateCameraSensitivityButtonVisual(button, isSelected)
                // Close camera menu when overlay is hidden (isSelected = false)
                if (!isSelected) {
                    isCameraMenuVisible = false
                    updateToolbarVisibility()
                }
            }

            // Bind free camera control button to overlay manager
            overlayManager.bindButton(freeCameraControlButton, "free_camera_control") { button, isSelected ->
                updateFreeCameraControlButtonVisual(button, isSelected)
                // Close camera menu when overlay is hidden (isSelected = false)
                if (!isSelected) {
                    isCameraMenuVisible = false
                    updateToolbarVisibility()
                }
            }

            // Color picker button with color indicator
            colorPickerButton = createColorPickerButton(activity) {
                Log.d(TAG, "Color picker button clicked")
                // Color picker is special - it shows immediately rather than being toggled
                val colorPickerOverlay = overlayManager.getOverlay("color_picker") as? ColorPickerOverlayUI
                colorPickerOverlay?.show(activity) { selectedColor ->
                    // Update color indicator when color is selected
                    updateColorIndicator(selectedColor)

                    // Send color to native code
                    try {
                        val red = Color.red(selectedColor) / 255f
                        val green = Color.green(selectedColor) / 255f
                        val blue = Color.blue(selectedColor) / 255f
                        val alpha = Color.alpha(selectedColor) / 255f
                        LinkuraHookMain.setCameraBackgroundColor(red, green, blue, alpha)
                        Log.d(TAG, "Camera background color set from toolbar: R=$red, G=$green, B=$blue, A=$alpha")
                    } catch (e: Exception) {
                        Log.e(TAG, "Error setting camera background color from toolbar", e)
                    }
                }
            }
            addView(colorPickerButton)

            // Collapse button
            addView(createMainToolButton(activity, SVGIcon.KeyboardArrowRight::createDrawable, "Collapse") {
                toggleExpansion()
            })
        }
    }

    private fun createMainToolButton(
        activity: Activity,
        iconCreator: (Context, Int, Float) -> Drawable,
        contentDescription: String,
        onClick: () -> Unit
    ): ImageButton {
        return ImageButton(activity).apply {
            val size = (40f * activity.resources.displayMetrics.density).toInt()
            layoutParams = LinearLayout.LayoutParams(size, size).apply {
                rightMargin = (12f * activity.resources.displayMetrics.density).toInt()
            }
            setBackgroundColor(Color.TRANSPARENT)
            setImageDrawable(iconCreator(activity, Color.WHITE, 20f))
            scaleType = ImageView.ScaleType.CENTER

            setOnClickListener { onClick() }
        }
    }

    private fun createCameraSecondaryMenu(activity: Activity) {
        cameraSecondaryMenu = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            visibility = View.GONE

            val cornerRadius = 8f * activity.resources.displayMetrics.density
            background = createRoundedBackground(Color.parseColor("#CC000000"), cornerRadius)

            val padding = (4f * activity.resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
            gravity = Gravity.CENTER_VERTICAL or Gravity.CENTER_HORIZONTAL

            // Camera Info button
            cameraInfoButton = createSecondaryMenuButton(activity, SVGIcon.Info::createDrawable, "Camera Info") {}
            addView(cameraInfoButton)

            // Camera Sensitivity button
            cameraSensitivityButton = createSecondaryMenuButton(activity, SVGIcon.Settings::createDrawable, "Camera Sensitivity") {}
            addView(cameraSensitivityButton)

            // Free Camera Control button
            freeCameraControlButton = createSecondaryMenuButton(activity, SVGIcon.Gamepad::createDrawable, "Free Camera Control") {}
            addView(freeCameraControlButton)
        }
    }

    private fun createSecondaryMenuButton(
        activity: Activity,
        iconCreator: (Context, Int, Float) -> Drawable,
        contentDescription: String,
        onClick: () -> Unit
    ): ImageButton {
        return ImageButton(activity).apply {
            val size = (32f * activity.resources.displayMetrics.density).toInt()
            layoutParams = LinearLayout.LayoutParams(size, size).apply {
                rightMargin = (4f * activity.resources.displayMetrics.density).toInt()
            }
            setBackgroundColor(Color.TRANSPARENT)
            val cornerRadius = 6f * activity.resources.displayMetrics.density
            background = createRoundedBackground(Color.TRANSPARENT, cornerRadius)
            setImageDrawable(iconCreator(activity, Color.WHITE, 16f))
            scaleType = ImageView.ScaleType.CENTER

            setOnClickListener { onClick() }
        }
    }

    private fun createColorPickerButton(
        activity: Activity,
        onClick: () -> Unit
    ): FrameLayout {
        val density = activity.resources.displayMetrics.density
        val size = (40f * density).toInt()

        return FrameLayout(activity).apply {
            layoutParams = LinearLayout.LayoutParams(size, size).apply {
                rightMargin = (12f * density).toInt()
            }
            setBackgroundColor(Color.TRANSPARENT)

            // Main palette icon
            val paletteIcon = ImageView(activity).apply {
                layoutParams = FrameLayout.LayoutParams(
                    FrameLayout.LayoutParams.MATCH_PARENT,
                    FrameLayout.LayoutParams.MATCH_PARENT
                )
                setImageDrawable(SVGIcon.Palette.createDrawable(activity, Color.WHITE, 20f))
                scaleType = ImageView.ScaleType.CENTER
            }
            addView(paletteIcon)

            // Small color indicator in bottom-right corner
            colorIndicator = View(activity).apply {
                val indicatorSize = (12f * density).toInt()
                layoutParams = FrameLayout.LayoutParams(indicatorSize, indicatorSize, Gravity.BOTTOM or Gravity.END).apply {
                    rightMargin = (2f * density).toInt()
                    bottomMargin = (2f * density).toInt()
                }
                val cornerRadius = indicatorSize / 2f

                // Add a small white border
                val borderDrawable = createRoundedBackground(Color.parseColor("#3AC3FA"), cornerRadius)
                borderDrawable.setStroke((1f * density).toInt(), Color.WHITE)
                background = borderDrawable
            }
            addView(colorIndicator)

            setOnClickListener { onClick() }
        }
    }

    private fun updateColorIndicator(color: Int) {
        if (::colorIndicator.isInitialized) {
            val density = colorIndicator.context.resources.displayMetrics.density
            val indicatorSize = (12f * density).toInt()
            val cornerRadius = indicatorSize / 2f

            val borderDrawable = createRoundedBackground(color, cornerRadius)
            borderDrawable.setStroke((1f * density).toInt(), Color.WHITE)
            colorIndicator.background = borderDrawable
        }
    }

    private fun createRoundedBackground(color: Int, cornerRadius: Float): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            setColor(color)
            setCornerRadius(cornerRadius)
        }
    }

    private fun toggleCameraMenu() {
        isCameraMenuVisible = !isCameraMenuVisible
        updateToolbarVisibility()
        Log.d(TAG, "Camera menu visibility: $isCameraMenuVisible")
    }

    private fun updateToolbarVisibility() {
        if (::cameraSecondaryMenu.isInitialized) {
            cameraSecondaryMenu.visibility = if (isCameraMenuVisible && isExpanded) View.VISIBLE else View.GONE
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun setupTouchHandling() {
        overlayView.setOnTouchListener { view, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    initialX = (overlayView.layoutParams as WindowManager.LayoutParams).x
                    initialY = (overlayView.layoutParams as WindowManager.LayoutParams).y
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

    private fun toggleExpansion() {
        isExpanded = !isExpanded
        isCameraMenuVisible = false // 折叠时关闭二级菜单

        // 折叠时关闭所有overlay
        if (!isExpanded) {
            overlayManager.hideAllOverlays()
        }

        // 重新创建UI以切换状态
        if (::toolbarContainer.isInitialized) {
            toolbarContainer.removeAllViews()

            if (isExpanded) {
                // 展开状态：显示完整工具栏
                val activity = overlayView.context as Activity

                // Recreate all components to avoid parent conflicts
                createControlBar(activity)
                createMainToolbar(activity)
                createCameraSecondaryMenu(activity)

                toolbarContainer.addView(createControlBarContainer(activity))
                toolbarContainer.addView(createMainToolbarContainer(activity))
                toolbarContainer.addView(cameraSecondaryMenu)

                // Rebind buttons after recreation
                overlayManager.bindButton(archiveButton, "archive") { button, isSelected ->
                    updateArchiveButtonVisual(button, isSelected)
                }
                overlayManager.bindButton(cameraInfoButton, "camera_info") { button, isSelected ->
                    updateCameraInfoButtonVisual(button, isSelected)
                    // Close camera menu when overlay is hidden (isSelected = false)
                    if (!isSelected) {
                        isCameraMenuVisible = false
                        updateToolbarVisibility()
                    }
                }
                overlayManager.bindButton(cameraSensitivityButton, "camera_sensitivity") { button, isSelected ->
                    updateCameraSensitivityButtonVisual(button, isSelected)
                    // Close camera menu when overlay is hidden (isSelected = false)
                    if (!isSelected) {
                        isCameraMenuVisible = false
                        updateToolbarVisibility()
                    }
                }
                overlayManager.bindButton(freeCameraControlButton, "free_camera_control") { button, isSelected ->
                    updateFreeCameraControlButtonVisual(button, isSelected)
                    // Close camera menu when overlay is hidden (isSelected = false)
                    if (!isSelected) {
                        isCameraMenuVisible = false
                        updateToolbarVisibility()
                    }
                }
            } else {
                // 折叠状态：只显示拖拽手柄和展开按钮
                val activity = overlayView.context as Activity
                toolbarContainer.addView(createCollapsedContainer(activity))
            }
        }

        updateToolbarVisibility()
        Log.d(TAG, "Toolbar ${if (isExpanded) "expanded" else "collapsed"}")
    }

    private fun createCollapsedContainer(activity: Activity): LinearLayout {
        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )

            val cornerRadius = 16f * activity.resources.displayMetrics.density
            background = createRoundedBackground(Color.parseColor("#CC000000"), cornerRadius)

            val padding = (6f * activity.resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
            gravity = Gravity.CENTER_VERTICAL

            // Drag handle for collapsed state
            val dragHandle = ImageView(activity).apply {
                val size = (24f * activity.resources.displayMetrics.density).toInt()
                layoutParams = LinearLayout.LayoutParams(size, size).apply {
                    rightMargin = (4f * activity.resources.displayMetrics.density).toInt()
                }
                setImageDrawable(SVGIcon.DragIndicator.createDrawable(activity, Color.argb(179, 255, 255, 255), 14f))
                scaleType = ImageView.ScaleType.CENTER
            }
            addView(dragHandle)

            // Expand button
            val expandButton = ImageButton(activity).apply {
                val size = (24f * activity.resources.displayMetrics.density).toInt()
                layoutParams = LinearLayout.LayoutParams(size, size)
                setBackgroundColor(Color.TRANSPARENT)
                setImageDrawable(SVGIcon.KeyboardArrowLeft.createDrawable(activity, Color.WHITE, 16f))
                scaleType = ImageView.ScaleType.CENTER

                setOnClickListener {
                    toggleExpansion()
                }
            }
            addView(expandButton)
        }
    }

    private fun setupOverlays() {
        // Register archive overlay
        val archiveOverlay = ArchiveOverlayUI()
        overlayManager.registerOverlay(archiveOverlay)

        // Register camera info overlay
        val cameraInfoOverlay = CameraInfoOverlayUI()
        overlayManager.registerOverlay(cameraInfoOverlay)

        // Register camera sensitivity overlay
        val cameraSensitivityOverlay = CameraSensitivityOverlayUI()
        overlayManager.registerOverlay(cameraSensitivityOverlay)

        // Register free camera control overlay
        val freeCameraControlOverlay = FreeCameraControlOverlayUI()
        overlayManager.registerOverlay(freeCameraControlOverlay)

        // Register color picker overlay
        val colorPickerOverlay = ColorPickerOverlayUI()
        overlayManager.registerOverlay(colorPickerOverlay)

        // Add more overlays here in the future:
        // val customOverlay = XposedCustomOverlayUI()
        // overlayManager.registerOverlay(customOverlay)
    }

    fun removeOverlay() {
        if (!isCreated) return
        isCreated = false

        try {
            windowManager.removeView(overlayView)
            overlayManager.destroy()
            Log.d(TAG, "Overlay toolbar removed")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to remove overlay toolbar", e)
        }
    }

    private fun updateArchiveButtonVisual(button: ImageButton, isSelected: Boolean) {
        val context = button.context
        if (isSelected) {
            // Selected state - blue background
            val cornerRadius = 6f * context.resources.displayMetrics.density
            button.background = createRoundedBackground(Color.parseColor("#FF67DAFC"), cornerRadius)
            button.setImageDrawable(SVGIcon.PlayArrow.createDrawable(context, Color.WHITE, 20f))
        } else {
            // Normal state - transparent background
            button.setBackgroundColor(Color.TRANSPARENT)
            button.setImageDrawable(SVGIcon.PlayArrow.createDrawable(context, Color.WHITE, 20f))
        }
    }

    private fun updateCameraInfoButtonVisual(button: ImageButton, isSelected: Boolean) {
        val context = button.context
        if (isSelected) {
            // Selected state - blue background
            val cornerRadius = 6f * context.resources.displayMetrics.density
            button.background = createRoundedBackground(Color.parseColor("#FF67DAFC"), cornerRadius)
            button.setImageDrawable(SVGIcon.Info.createDrawable(context, Color.WHITE, 16f))
        } else {
            // Normal state - transparent background
            button.setBackgroundColor(Color.TRANSPARENT)
            button.setImageDrawable(SVGIcon.Info.createDrawable(context, Color.WHITE, 16f))
        }
    }

    private fun updateCameraSensitivityButtonVisual(button: ImageButton, isSelected: Boolean) {
        val context = button.context
        if (isSelected) {
            // Selected state - blue background
            val cornerRadius = 6f * context.resources.displayMetrics.density
            button.background = createRoundedBackground(Color.parseColor("#FF67DAFC"), cornerRadius)
            button.setImageDrawable(SVGIcon.Settings.createDrawable(context, Color.WHITE, 16f))
        } else {
            // Normal state - transparent background
            button.setBackgroundColor(Color.TRANSPARENT)
            button.setImageDrawable(SVGIcon.Settings.createDrawable(context, Color.WHITE, 16f))
        }
    }

    private fun updateFreeCameraControlButtonVisual(button: ImageButton, isSelected: Boolean) {
        val context = button.context
        if (isSelected) {
            // Selected state - blue background
            val cornerRadius = 6f * context.resources.displayMetrics.density
            button.background = createRoundedBackground(Color.parseColor("#FF67DAFC"), cornerRadius)
            button.setImageDrawable(SVGIcon.Gamepad.createDrawable(context, Color.WHITE, 16f))
        } else {
            // Normal state - transparent background
            button.setBackgroundColor(Color.TRANSPARENT)
            button.setImageDrawable(SVGIcon.Gamepad.createDrawable(context, Color.WHITE, 16f))
        }
    }

    fun isOverlayCreated(): Boolean = isCreated
}