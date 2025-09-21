package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.text.Editable
import android.text.TextWatcher
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.*
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.LinkuraHookMain

class ColorPickerOverlayUI : BaseOverlay {
    override val overlayId = "color_picker"
    override val displayName = "Color Picker"

    override var onOverlayHidden: (() -> Unit)? = null
    override var onVisibilityChanged: ((Boolean) -> Unit)? = null

    private var isCreated = false
    private var isVisible = false
    private lateinit var windowManager: WindowManager
    private lateinit var overlayView: FrameLayout

    // Color state
    private var currentColor = Color.parseColor("#3AC3FA") // Default blue
    private var redValue = 58
    private var greenValue = 195
    private var blueValue = 250
    private var alphaValue = 255

    // Color mode state
    private var isRGBMode = true // true for RGB, false for HSL
    private var hueValue = 0f
    private var saturationValue = 0f
    private var lightnessValue = 0f

    // UI Components
    private lateinit var colorPreview: View
    private lateinit var hexDisplayText: TextView
    private lateinit var rgbTabButton: Button
    private lateinit var hslTabButton: Button
    private lateinit var colorSlidersContainer: LinearLayout

    // RGB components
    private lateinit var redSeekBar: SeekBar
    private lateinit var greenSeekBar: SeekBar
    private lateinit var blueSeekBar: SeekBar
    private lateinit var alphaSeekBar: SeekBar
    private lateinit var redValueText: TextView
    private lateinit var greenValueText: TextView
    private lateinit var blueValueText: TextView
    private lateinit var alphaValueText: TextView

    // HSL components
    private lateinit var hueSeekBar: SeekBar
    private lateinit var saturationSeekBar: SeekBar
    private lateinit var lightnessSeekBar: SeekBar
    private lateinit var hueValueText: TextView
    private lateinit var saturationValueText: TextView
    private lateinit var lightnessValueText: TextView

    private var onColorSelected: ((Int) -> Unit)? = null

    fun show(context: Context, initialColor: Int = currentColor, onColorSelected: (Int) -> Unit) {
        if (isVisible) return

        val activity = context as? Activity ?: return
        this.currentColor = initialColor
        this.onColorSelected = onColorSelected

        // Extract RGBA values
        updateColorValues(initialColor)
        updateHSLValues(initialColor)

        try {
            createOverlay(activity)
            isVisible = true
            onVisibilityChanged?.invoke(true)
            Log.d(TAG, "Color picker overlay shown")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing color picker overlay", e)
        }
    }

    fun show(context: Context, onColorSelected: (Int) -> Unit) {
        show(context, currentColor, onColorSelected)
    }

    override fun show(context: Context) {
        show(context, currentColor) { color ->
            // Default color selection handler - send to native
            try {
                val red = Color.red(color) / 255f
                val green = Color.green(color) / 255f
                val blue = Color.blue(color) / 255f
                val alpha = Color.alpha(color) / 255f
                LinkuraHookMain.setCameraBackgroundColor(red, green, blue, alpha)
                Log.d(TAG, "Camera background color set: R=$red, G=$green, B=$blue, A=$alpha")
            } catch (e: Exception) {
                Log.e(TAG, "Error setting camera background color", e)
            }
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
            Log.d(TAG, "Color picker overlay hidden")
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding color picker overlay", e)
        }
    }

    override fun isCreated(): Boolean = isCreated
    override fun isVisible(): Boolean = isVisible

    @SuppressLint("ClickableViewAccessibility")
    private fun createOverlay(activity: Activity) {
        windowManager = activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager
        val density = activity.resources.displayMetrics.density

        // Create main overlay container (fullscreen with semi-transparent background)
        overlayView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
            setBackgroundColor(Color.parseColor("#80000000")) // Semi-transparent black

            // Click outside to close
            setOnClickListener {
                hide()
            }
        }

        // Create content container
        val contentContainer = createContentContainer(activity)

        // Center the content
        val contentParams = FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.CENTER
        )
        overlayView.addView(contentContainer, contentParams)

        // Add overlay to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.MATCH_PARENT
            height = WindowManager.LayoutParams.MATCH_PARENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.CENTER
        }

        try {
            windowManager.addView(overlayView, layoutParams)
            isCreated = true
            Log.d(TAG, "Color picker overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create color picker overlay", e)
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createContentContainer(activity: Activity): ViewGroup {
        val density = activity.resources.displayMetrics.density
        val isLandscape = activity.resources.configuration.orientation ==
            android.content.res.Configuration.ORIENTATION_LANDSCAPE

        return if (isLandscape) {
            createLandscapeLayout(activity)
        } else {
            createPortraitLayout(activity)
        }
    }

    private fun createPortraitLayout(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = ViewGroup.LayoutParams(
                (320f * density).toInt(),
                ViewGroup.LayoutParams.WRAP_CONTENT
            )

            val cornerRadius = 16f * density
            background = createRoundedBackground(Color.parseColor("#F5F5F5"), cornerRadius)

            val padding = (16f * density).toInt()
            setPadding(padding, padding, padding, padding)

            // Prevent clicks from passing through to the background
            setOnClickListener { /* Consume click */ }

            // Title
            addView(createTitleSection(activity))

            // Color preview and hex input
            addView(createColorPreviewSection(activity))

            // Color sliders (RGB or HSL)
            colorSlidersContainer = createColorSlidersSection(activity)
            addView(colorSlidersContainer)

            // Preset colors
            addView(createPresetColorsSection(activity))

            // Buttons
            addView(createButtonsSection(activity))
        }
    }

    private fun createLandscapeLayout(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = ViewGroup.LayoutParams(
                (480f * density).toInt(), // Wider for landscape
                ViewGroup.LayoutParams.WRAP_CONTENT
            )

            val cornerRadius = 16f * density
            background = createRoundedBackground(Color.parseColor("#F5F5F5"), cornerRadius)

            val padding = (16f * density).toInt()
            setPadding(padding, padding, padding, padding)

            // Prevent clicks from passing through to the background
            setOnClickListener { /* Consume click */ }

            // Left side: Controls
            val leftColumn = LinearLayout(activity).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams = LinearLayout.LayoutParams(
                    0,
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    1f
                ).apply {
                    rightMargin = (16f * density).toInt()
                }

                // Title
                addView(createTitleSection(activity))

                // Color preview and hex input
                addView(createColorPreviewSection(activity))

                // Color sliders (RGB or HSL)
                colorSlidersContainer = createColorSlidersSection(activity)
                addView(colorSlidersContainer)

                // Buttons
                addView(createButtonsSection(activity))
            }
            addView(leftColumn)

            // Right side: Preset colors
            val rightColumn = LinearLayout(activity).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams = LinearLayout.LayoutParams(
                    (100f * density).toInt(),
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )

                // Preset colors section for landscape
                addView(createPresetColorsLandscapeSection(activity))
            }
            addView(rightColumn)
        }
    }

    private fun createTitleSection(activity: Activity): TextView {
        val density = activity.resources.displayMetrics.density

        return TextView(activity).apply {
            text = "Color Picker"
            textSize = 18f
            setTextColor(Color.BLACK)
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (16f * density).toInt()
            }
        }
    }

    private fun createColorPreviewSection(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (16f * density).toInt()
            }

            // Color preview (smaller)
            colorPreview = View(activity).apply {
                val size = (32f * density).toInt() // Reduced from 60f to 32f
                layoutParams = LinearLayout.LayoutParams(size, size).apply {
                    rightMargin = (12f * density).toInt()
                }
                val cornerRadius = 6f * density
                background = createRoundedBackground(currentColor, cornerRadius)
            }
            addView(colorPreview)

            // RGB/HSL Tab section
            val tabSection = LinearLayout(activity).apply {
                orientation = LinearLayout.VERTICAL
                layoutParams = LinearLayout.LayoutParams(
                    0,
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    1f
                )

                // Tab buttons
                val tabContainer = LinearLayout(activity).apply {
                    orientation = LinearLayout.HORIZONTAL
                    layoutParams = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    )

                    rgbTabButton = Button(activity).apply {
                        text = "RGB"
                        textSize = 12f
                        setTextColor(if (isRGBMode) Color.WHITE else Color.BLACK)
                        layoutParams = LinearLayout.LayoutParams(
                            0,
                            (32f * density).toInt(),
                            1f
                        ).apply {
                            rightMargin = (2f * density).toInt()
                        }
                        val cornerRadius = 4f * density
                        background = createRoundedBackground(
                            if (isRGBMode) Color.parseColor("#007ACC") else Color.parseColor("#E0E0E0"),
                            cornerRadius
                        )
                        setPadding(0, 0, 0, 0) // Remove default button padding
                        setOnClickListener {
                            if (!isRGBMode) toggleColorMode()
                        }
                    }
                    addView(rgbTabButton)

                    hslTabButton = Button(activity).apply {
                        text = "HSL"
                        textSize = 12f
                        setTextColor(if (!isRGBMode) Color.WHITE else Color.BLACK)
                        layoutParams = LinearLayout.LayoutParams(
                            0,
                            (32f * density).toInt(),
                            1f
                        ).apply {
                            leftMargin = (2f * density).toInt()
                        }
                        val cornerRadius = 4f * density
                        background = createRoundedBackground(
                            if (!isRGBMode) Color.parseColor("#007ACC") else Color.parseColor("#E0E0E0"),
                            cornerRadius
                        )
                        setPadding(0, 0, 0, 0) // Remove default button padding
                        setOnClickListener {
                            if (isRGBMode) toggleColorMode()
                        }
                    }
                    addView(hslTabButton)
                }
                addView(tabContainer)

                // Hex display (read-only, top right)
                hexDisplayText = TextView(activity).apply {
                    text = colorToHex(currentColor)
                    textSize = 10f
                    setTextColor(Color.GRAY)
                    gravity = Gravity.END
                    layoutParams = LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.WRAP_CONTENT
                    ).apply {
                        bottomMargin = (4f * density).toInt()
                    }
                }
                addView(hexDisplayText)
            }
            addView(tabSection)
        }
    }

    private fun createColorSlidersSection(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (16f * density).toInt()
            }

            if (isRGBMode) {
                createRGBSliders(activity, this)
            } else {
                createHSLSliders(activity, this)
            }

            // Alpha slider (always present)
            val alphaSliderTriple = createColorSlider(activity, "A", alphaValue, Color.GRAY, 255) { value ->
                alphaValue = value
                updateColor()
            }
            addView(alphaSliderTriple.first)
            alphaSeekBar = alphaSliderTriple.second
            alphaValueText = alphaSliderTriple.third
        }
    }

    private fun createColorSlider(
        activity: Activity,
        label: String,
        initialValue: Int,
        color: Int,
        maxValue: Int = 255,
        onValueChange: (Int) -> Unit
    ): Triple<LinearLayout, SeekBar, TextView> {
        val density = activity.resources.displayMetrics.density

        // Single row layout: Label - Slider - Value
        val container = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                (32f * density).toInt() // Fixed height for compact slider
            ).apply {
                bottomMargin = (4f * density).toInt() // Reduced margin
            }
            gravity = Gravity.CENTER_VERTICAL
        }

        // Label (single letter)
        val labelText = TextView(activity).apply {
            text = label
            textSize = 14f
            setTextColor(Color.BLACK)
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(
                (20f * density).toInt(), // Fixed width for single letter
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            gravity = Gravity.CENTER
        }
        container.addView(labelText)

        // Slider
        val seekBar = SeekBar(activity).apply {
            max = maxValue
            progress = initialValue
            layoutParams = LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1f
            ).apply {
                leftMargin = (8f * density).toInt()
                rightMargin = (8f * density).toInt()
            }

            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    if (fromUser) {
                        onValueChange(progress)
                    }
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) {}
                override fun onStopTrackingTouch(seekBar: SeekBar?) {}
            })
        }
        container.addView(seekBar)

        // Value display
        val valueText = TextView(activity).apply {
            text = initialValue.toString()
            textSize = 12f
            setTextColor(Color.BLACK)
            gravity = Gravity.CENTER
            layoutParams = LinearLayout.LayoutParams(
                (32f * density).toInt(), // Fixed width for number
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }
        container.addView(valueText)

        // Update value text in change listener
        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    valueText.text = progress.toString()
                    onValueChange(progress)
                }
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })

        return Triple(container, seekBar, valueText)
    }

    private fun createPresetColorsSection(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (16f * density).toInt()
            }

            val label = TextView(activity).apply {
                text = "Preset Colors"
                textSize = 12f
                setTextColor(Color.BLACK)
                typeface = android.graphics.Typeface.DEFAULT_BOLD
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    bottomMargin = (8f * density).toInt()
                }
            }
            addView(label)

            val presetColors = listOf(
                Color.parseColor("#00FF00"), // Green
                Color.parseColor("#0000FF"), // Blue
                Color.parseColor("#FF0000"), // Red
                Color.parseColor("#000000"), // Black
                Color.parseColor("#FFFFFF"), // White
                Color.parseColor("#3AC3FA")  // Default blue
            )

            val colorsRow = LinearLayout(activity).apply {
                orientation = LinearLayout.HORIZONTAL
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                )

                presetColors.forEachIndexed { index, color ->
                    val colorBox = View(activity).apply {
                        val size = (36f * density).toInt()
                        layoutParams = LinearLayout.LayoutParams(size, size).apply {
                            if (index < presetColors.size - 1) {
                                rightMargin = (8f * density).toInt()
                            }
                        }
                        val cornerRadius = 4f * density
                        background = createBorderedColorBackground(color, cornerRadius, density)

                        setOnClickListener {
                            updateColorFromInt(color)
                        }
                    }
                    addView(colorBox)
                }
            }
            addView(colorsRow)
        }
    }

    private fun createButtonsSection(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )

            // Cancel button
            val cancelButton = Button(activity).apply {
                text = "Cancel"
                setTextColor(Color.BLACK)
                layoutParams = LinearLayout.LayoutParams(
                    0,
                    (48f * density).toInt(),
                    1f
                ).apply {
                    rightMargin = (8f * density).toInt()
                }
                val cornerRadius = 8f * density
                background = createRoundedBackground(Color.parseColor("#E0E0E0"), cornerRadius)

                setOnClickListener {
                    hide()
                }
            }
            addView(cancelButton)

            // Confirm button
            val confirmButton = Button(activity).apply {
                text = "Confirm"
                setTextColor(Color.WHITE)
                layoutParams = LinearLayout.LayoutParams(
                    0,
                    (48f * density).toInt(),
                    1f
                )
                val cornerRadius = 8f * density
                background = createRoundedBackground(Color.parseColor("#007ACC"), cornerRadius)

                setOnClickListener {
                    onColorSelected?.invoke(currentColor)
                    hide()
                }
            }
            addView(confirmButton)
        }
    }

    private fun updateColor() {
        currentColor = Color.argb(alphaValue, redValue, greenValue, blueValue)
        updateUI()
    }

    private fun updateColorFromInt(color: Int) {
        currentColor = color
        updateColorValues(color)
        updateUI()
        updateSliders()
    }

    private fun updateColorValues(color: Int) {
        redValue = Color.red(color)
        greenValue = Color.green(color)
        blueValue = Color.blue(color)
        alphaValue = Color.alpha(color)
    }

    private fun updateUI() {
        if (::colorPreview.isInitialized) {
            val cornerRadius = 8f * colorPreview.context.resources.displayMetrics.density
            colorPreview.background = createRoundedBackground(currentColor, cornerRadius)
        }

        if (::hexDisplayText.isInitialized) {
            val hex = colorToHex(currentColor)
            if (hexDisplayText.text.toString() != hex) {
                hexDisplayText.text = hex
            }
        }
    }

    private fun updateSliders() {
        if (isRGBMode) {
            if (::redSeekBar.isInitialized) {
                redSeekBar.progress = redValue
                redValueText.text = redValue.toString()
            }
            if (::greenSeekBar.isInitialized) {
                greenSeekBar.progress = greenValue
                greenValueText.text = greenValue.toString()
            }
            if (::blueSeekBar.isInitialized) {
                blueSeekBar.progress = blueValue
                blueValueText.text = blueValue.toString()
            }
        } else {
            if (::hueSeekBar.isInitialized) {
                hueSeekBar.progress = hueValue.toInt()
                hueValueText.text = hueValue.toInt().toString()
            }
            if (::saturationSeekBar.isInitialized) {
                saturationSeekBar.progress = (saturationValue * 100).toInt()
                saturationValueText.text = (saturationValue * 100).toInt().toString()
            }
            if (::lightnessSeekBar.isInitialized) {
                lightnessSeekBar.progress = (lightnessValue * 100).toInt()
                lightnessValueText.text = (lightnessValue * 100).toInt().toString()
            }
        }

        if (::alphaSeekBar.isInitialized) {
            alphaSeekBar.progress = alphaValue
            alphaValueText.text = alphaValue.toString()
        }
    }

    private fun createRoundedBackground(color: Int, cornerRadius: Float): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            setColor(color)
            setCornerRadius(cornerRadius)
        }
    }

    private fun createBorderedColorBackground(color: Int, cornerRadius: Float, density: Float): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            setColor(color)
            setCornerRadius(cornerRadius)
            setStroke((1f * density).toInt(), Color.parseColor("#CCCCCC"))
        }
    }

    private fun createRGBSliders(activity: Activity, container: LinearLayout) {
        // Red slider
        val redSliderTriple = createColorSlider(activity, "R", redValue, Color.RED) { value ->
            redValue = value
            updateColor()
        }
        container.addView(redSliderTriple.first)
        redSeekBar = redSliderTriple.second
        redValueText = redSliderTriple.third

        // Green slider
        val greenSliderTriple = createColorSlider(activity, "G", greenValue, Color.GREEN) { value ->
            greenValue = value
            updateColor()
        }
        container.addView(greenSliderTriple.first)
        greenSeekBar = greenSliderTriple.second
        greenValueText = greenSliderTriple.third

        // Blue slider
        val blueSliderTriple = createColorSlider(activity, "B", blueValue, Color.BLUE) { value ->
            blueValue = value
            updateColor()
        }
        container.addView(blueSliderTriple.first)
        blueSeekBar = blueSliderTriple.second
        blueValueText = blueSliderTriple.third
    }

    private fun createHSLSliders(activity: Activity, container: LinearLayout) {
        // Hue slider
        val hueSliderTriple = createColorSlider(activity, "H", hueValue.toInt(), Color.parseColor("#FF0000"), 360) { value ->
            hueValue = value.toFloat()
            updateColorFromHSL()
        }
        container.addView(hueSliderTriple.first)
        hueSeekBar = hueSliderTriple.second
        hueValueText = hueSliderTriple.third

        // Saturation slider
        val satSliderTriple = createColorSlider(activity, "S", (saturationValue * 100).toInt(), Color.parseColor("#FF00FF"), 100) { value ->
            saturationValue = value / 100f
            updateColorFromHSL()
        }
        container.addView(satSliderTriple.first)
        saturationSeekBar = satSliderTriple.second
        saturationValueText = satSliderTriple.third

        // Lightness slider
        val lightSliderTriple = createColorSlider(activity, "L", (lightnessValue * 100).toInt(), Color.parseColor("#808080"), 100) { value ->
            lightnessValue = value / 100f
            updateColorFromHSL()
        }
        container.addView(lightSliderTriple.first)
        lightnessSeekBar = lightSliderTriple.second
        lightnessValueText = lightSliderTriple.third
    }

    private fun toggleColorMode() {
        isRGBMode = !isRGBMode

        // Update tab button styles
        val density = (colorSlidersContainer.context as Activity).resources.displayMetrics.density
        val cornerRadius = 4f * density

        rgbTabButton.apply {
            setTextColor(if (isRGBMode) Color.WHITE else Color.BLACK)
            background = createRoundedBackground(
                if (isRGBMode) Color.parseColor("#007ACC") else Color.parseColor("#E0E0E0"),
                cornerRadius
            )
            setPadding(0, 0, 0, 0) // Remove default button padding
        }

        hslTabButton.apply {
            setTextColor(if (!isRGBMode) Color.WHITE else Color.BLACK)
            background = createRoundedBackground(
                if (!isRGBMode) Color.parseColor("#007ACC") else Color.parseColor("#E0E0E0"),
                cornerRadius
            )
            setPadding(0, 0, 0, 0) // Remove default button padding
        }

        // Recreate the sliders section
        colorSlidersContainer.removeAllViews()
        if (isRGBMode) {
            createRGBSliders(colorSlidersContainer.context as Activity, colorSlidersContainer)
        } else {
            createHSLSliders(colorSlidersContainer.context as Activity, colorSlidersContainer)
        }

        // Add alpha slider back
        val alphaSliderTriple = createColorSlider(colorSlidersContainer.context as Activity, "A", alphaValue, Color.GRAY, 255) { value ->
            alphaValue = value
            updateColor()
        }
        colorSlidersContainer.addView(alphaSliderTriple.first)
        alphaSeekBar = alphaSliderTriple.second
        alphaValueText = alphaSliderTriple.third
    }

    private fun updateColorFromHSL() {
        val rgb = hslToRgb(hueValue, saturationValue, lightnessValue)
        redValue = rgb[0]
        greenValue = rgb[1]
        blueValue = rgb[2]
        updateColor()
    }

    private fun updateHSLValues(color: Int) {
        val r = Color.red(color) / 255f
        val g = Color.green(color) / 255f
        val b = Color.blue(color) / 255f

        val hsl = rgbToHsl(r, g, b)
        hueValue = hsl[0]
        saturationValue = hsl[1]
        lightnessValue = hsl[2]
    }

    private fun rgbToHsl(r: Float, g: Float, b: Float): FloatArray {
        val max = maxOf(r, g, b)
        val min = minOf(r, g, b)
        val delta = max - min

        var h = 0f
        var s = 0f
        val l = (max + min) / 2f

        if (delta != 0f) {
            s = if (l < 0.5f) delta / (max + min) else delta / (2f - max - min)

            h = when (max) {
                r -> ((g - b) / delta + if (g < b) 6f else 0f) / 6f
                g -> ((b - r) / delta + 2f) / 6f
                b -> ((r - g) / delta + 4f) / 6f
                else -> 0f
            }
        }

        return floatArrayOf(h * 360f, s, l)
    }

    private fun hslToRgb(h: Float, s: Float, l: Float): IntArray {
        val hNorm = h / 360f
        val c = (1f - kotlin.math.abs(2f * l - 1f)) * s
        val x = c * (1f - kotlin.math.abs((hNorm * 6f) % 2f - 1f))
        val m = l - c / 2f

        val (rPrime, gPrime, bPrime) = when ((hNorm * 6f).toInt()) {
            0 -> Triple(c, x, 0f)
            1 -> Triple(x, c, 0f)
            2 -> Triple(0f, c, x)
            3 -> Triple(0f, x, c)
            4 -> Triple(x, 0f, c)
            else -> Triple(c, 0f, x)
        }

        val r = ((rPrime + m) * 255f).toInt().coerceIn(0, 255)
        val g = ((gPrime + m) * 255f).toInt().coerceIn(0, 255)
        val b = ((bPrime + m) * 255f).toInt().coerceIn(0, 255)

        return intArrayOf(r, g, b)
    }

    private fun colorToHex(color: Int): String {
        return String.format("%08X", color)
    }

    private fun hexToColor(hex: String): Int? {
        return try {
            val cleanHex = hex.replace("#", "").trim().uppercase()

            // Validate hex characters
            if (!cleanHex.all { it in '0'..'9' || it in 'A'..'F' }) {
                return null
            }

            when (cleanHex.length) {
                6 -> {
                    // 6-char hex color (RGB)
                    val rgb = cleanHex.toLong(16).toInt()
                    Color.rgb(
                        (rgb shr 16) and 0xFF,
                        (rgb shr 8) and 0xFF,
                        rgb and 0xFF
                    )
                }
                8 -> {
                    // 8-char hex color (ARGB)
                    cleanHex.toLong(16).toInt()
                }
                else -> null
            }
        } catch (e: Exception) {
            null
        }
    }

    private fun createPresetColorsLandscapeSection(activity: Activity): LinearLayout {
        val density = activity.resources.displayMetrics.density

        return LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.MATCH_PARENT
            )

            val label = TextView(activity).apply {
                text = "Presets"
                textSize = 12f
                setTextColor(Color.BLACK)
                typeface = android.graphics.Typeface.DEFAULT_BOLD
                gravity = Gravity.CENTER
                layoutParams = LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT
                ).apply {
                    bottomMargin = (8f * density).toInt()
                }
            }
            addView(label)

            val presetColors = listOf(
                Color.parseColor("#00FF00"), // Green
                Color.parseColor("#0000FF"), // Blue
                Color.parseColor("#FF0000"), // Red
                Color.parseColor("#000000"), // Black
                Color.parseColor("#FFFFFF"), // White
                Color.parseColor("#3AC3FA")  // Default blue
            )

            // Vertical arrangement of color boxes for landscape (square)
            presetColors.forEach { color ->
                val colorBox = View(activity).apply {
                    val size = (32f * density).toInt()
                    layoutParams = LinearLayout.LayoutParams(
                        size, // Fixed width instead of MATCH_PARENT for square shape
                        size
                    ).apply {
                        bottomMargin = (4f * density).toInt()
                        gravity = Gravity.CENTER_HORIZONTAL
                    }
                    val cornerRadius = 4f * density
                    background = createBorderedColorBackground(color, cornerRadius, density)

                    setOnClickListener {
                        updateColorFromInt(color)
                    }
                }
                addView(colorBox)
            }
        }
    }

    override fun destroy() {
        hide()
        super.destroy()
    }
}