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
import android.widget.TextView
import io.github.chocolzs.linkura.localify.TAG

class OverlayToolbarUI {
    private var isCreated = false
    private var isExpanded = false
    private lateinit var windowManager: WindowManager
    private lateinit var overlayView: FrameLayout
    private lateinit var toolbarContainer: LinearLayout
    private lateinit var toggleButton: ImageButton
    private lateinit var expandedContent: LinearLayout

    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f

    @SuppressLint("ClickableViewAccessibility")
    fun createOverlay(context: Context) {
        if (isCreated) return
        isCreated = true

        val activity = context as? Activity ?: return

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

        // Create toolbar container with transparent background
        toolbarContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )
            setBackgroundColor(Color.TRANSPARENT)
            setPadding(8, 8, 8, 8)
        }

        // Create toggle button (always visible)
        val toggleButtonSize = (70f * activity.resources.displayMetrics.density).toInt()
        val toggleButtonPadding = (12f * activity.resources.displayMetrics.density).toInt()

        toggleButton = ImageButton(activity).apply {
            layoutParams = LinearLayout.LayoutParams(toggleButtonSize, toggleButtonSize)
            setBackgroundColor(Color.parseColor("#80000000")) // Semi-transparent black
            scaleType = ImageView.ScaleType.CENTER
            setPadding(toggleButtonPadding, toggleButtonPadding, toggleButtonPadding, toggleButtonPadding)

            // Set menu icon
            setImageDrawable(SVGIcon.Menu.createDrawable(activity, Color.WHITE, 32f))

            setOnClickListener {
                toggleExpansion()
            }
        }

        // Create expanded content (initially hidden)
        expandedContent = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            setBackgroundColor(Color.parseColor("#CC000000")) // More opaque background for content
            setPadding(16, 16, 16, 16)
            visibility = View.GONE
        }

        // Add sample content to expanded area
        addSampleContent(activity)

        // Add views to container
        toolbarContainer.addView(toggleButton)
        toolbarContainer.addView(expandedContent)
        overlayView.addView(toolbarContainer)

        // Set up touch handling for dragging
        setupTouchHandling()

        // Add overlay to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.WRAP_CONTENT
            height = WindowManager.LayoutParams.WRAP_CONTENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.TOP or Gravity.START
            x = 100
            y = 200
        }

        try {
            windowManager.addView(overlayView, layoutParams)
            Log.d(TAG, "Overlay toolbar created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create overlay toolbar", e)
            isCreated = false
        }
    }

    private fun addSampleContent(activity: Activity) {
        // Add some sample buttons/content with icons
        val button1 = createToolButton(
            activity,
            SVGIcon.Camera::createDrawable,
            "Camera",
            Color.parseColor("#FF4CAF50")
        ) {
            Log.d(TAG, "Camera tool clicked")
        }

        val button2 = createToolButton(
            activity,
            SVGIcon.Settings::createDrawable,
            "Settings",
            Color.parseColor("#FF2196F3")
        ) {
            Log.d(TAG, "Settings tool clicked")
        }

        val button3 = createToolButton(
            activity,
            SVGIcon.Tool::createDrawable,
            "Tools",
            Color.parseColor("#FFFF9800")
        ) {
            Log.d(TAG, "Tools clicked")
        }

        expandedContent.addView(button1)
        expandedContent.addView(button2)
        expandedContent.addView(button3)
    }

    private fun createToolButton(
        activity: Activity,
        iconCreator: (Context, Int, Float) -> Drawable,
        text: String,
        backgroundColor: Int,
        onClick: () -> Unit
    ): LinearLayout {
        return LinearLayout(activity).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = 8
            }
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(backgroundColor)

            val containerPadding = (8f * activity.resources.displayMetrics.density).toInt()
            setPadding(containerPadding, containerPadding, containerPadding, containerPadding)
            gravity = Gravity.CENTER

            // Add icon
            val iconSize = (28f * activity.resources.displayMetrics.density).toInt()
            val containerSize = (40f * activity.resources.displayMetrics.density).toInt()

            val iconView = ImageView(activity).apply {
                layoutParams = LinearLayout.LayoutParams(containerSize, containerSize).apply {
                    gravity = Gravity.CENTER
                }
                scaleType = ImageView.ScaleType.CENTER

                setImageDrawable(iconCreator(activity, Color.WHITE, 28f))
            }

            addView(iconView)

            setOnClickListener { onClick() }
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
        expandedContent.visibility = if (isExpanded) View.VISIBLE else View.GONE

        // Update toggle button icon based on expansion state
        val context = toggleButton.context
        val iconDrawable = if (isExpanded) {
            SVGIcon.Close.createDrawable(context, Color.WHITE, 32f)
        } else {
            SVGIcon.Menu.createDrawable(context, Color.WHITE, 32f)
        }
        toggleButton.setImageDrawable(iconDrawable)

        // Update window layout params to handle click-through when collapsed
        val layoutParams = overlayView.layoutParams as WindowManager.LayoutParams
        if (!isExpanded) {
            // When collapsed, allow click-through for areas outside the toggle button
            layoutParams.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
        } else {
            // When expanded, capture touches for the expanded area
            layoutParams.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
        }

        try {
            windowManager.updateViewLayout(overlayView, layoutParams)
            Log.d(TAG, "Toolbar ${if (isExpanded) "expanded" else "collapsed"}")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to update overlay layout", e)
        }
    }

    fun removeOverlay() {
        if (!isCreated) return
        isCreated = false

        try {
            windowManager.removeView(overlayView)
            Log.d(TAG, "Overlay toolbar removed")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to remove overlay toolbar", e)
        }
    }

    fun isOverlayCreated(): Boolean = isCreated
}