package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.LinkuraHookMain
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*

class ArchiveOverlayUI : BaseOverlay {
    override val overlayId = "archive"
    override val displayName = "Archive Control"

    override var onOverlayHidden: (() -> Unit)? = null
    override var onVisibilityChanged: ((Boolean) -> Unit)? = null

    private var isCreated = false
    private var isVisible = false
    private lateinit var windowManager: WindowManager
    private lateinit var overlayView: FrameLayout
    private lateinit var contentContainer: LinearLayout
    private lateinit var titleText: TextView
    private lateinit var currentTimeText: TextView
    private lateinit var totalTimeText: TextView
    private lateinit var seekTimeText: TextView
    private lateinit var lastPositionText: TextView
    private lateinit var progressContainer: FrameLayout
    private lateinit var progressBar: View
    private lateinit var progressThumb: View

    private var archiveDuration = 0L // Duration in milliseconds
    private var currentPosition = 0f // Current position as percentage (0.0 to 1.0)
    private var isDragging = false
    private var tempPosition = 0f // Temporary position while dragging
    private var lastDraggedPosition = 0f // Remember last dragged position

    // Touch handling for window dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f

    // Progress bar dimensions (dp to pixels)
    private var progressBarHeight = 0
    private var progressBarWidth = 0
    private var thumbSize = 0

    @SuppressLint("ClickableViewAccessibility")
    override fun show(context: Context) {
        if (isVisible) return

        val activity = context as? Activity ?: return

        try {
            // Request archive info first
            requestArchiveInfo()
            createOverlay(activity)
            isVisible = true
            onVisibilityChanged?.invoke(true)
            Log.d(TAG, "Xposed archive overlay shown")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing Xposed archive overlay", e)
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
            Log.d(TAG, "Xposed archive overlay hidden")
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding Xposed archive overlay", e)
        }
    }

    fun updateArchiveDuration(duration: Long) {
        Log.d(TAG, "updateArchiveDuration called with duration: ${duration}ms")
        archiveDuration = duration
        if (::overlayView.isInitialized) {
            Log.d(TAG, "Overlay view is initialized, updating UI")
            updateUI()
        } else {
            Log.d(TAG, "Overlay view not initialized yet, will update when created")
        }
    }

    private fun requestArchiveInfo() {
        try {
            // Call native function to get archive info
            val archiveInfoBytes = LinkuraHookMain.getCurrentArchiveInfo()
            if (archiveInfoBytes.isNotEmpty()) {
                // Parse the protobuf data
                val archiveInfo = ArchiveInfo.parseFrom(archiveInfoBytes)
                archiveDuration = archiveInfo.duration
                Log.d(TAG, "Archive info received from native: duration=${archiveDuration}ms")

                // Update UI if overlay is already created
                if (::overlayView.isInitialized) {
                    updateUI()
                }
            } else {
                archiveDuration = 0L
                Log.d(TAG, "No archive info available from native")

                // Update UI if overlay is already created
                if (::overlayView.isInitialized) {
                    updateUI()
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error requesting archive info", e)
            archiveDuration = 0L
            if (::overlayView.isInitialized) {
                updateUI()
            }
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createOverlay(activity: Activity) {
        windowManager = activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager

        val density = activity.resources.displayMetrics.density
        progressBarHeight = (24f * density).toInt()
        progressBarWidth = (300f * density).toInt()
        thumbSize = (24f * density).toInt()

        // Create main overlay container
        overlayView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        // Create content container
        contentContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )

            val cornerRadius = 12f * density
            background = createRoundedBackground(Color.parseColor("#CC000000"), cornerRadius)

            val padding = (16f * density).toInt()
            setPadding(padding, padding, padding, padding)
        }

        createUI(activity)
        overlayView.addView(contentContainer)

        // Set up touch handling for window dragging
        setupTouchHandling(activity)

        // Update initial UI
        updateUI()

        // Add overlay to window manager
        val layoutParams = WindowManager.LayoutParams().apply {
            width = WindowManager.LayoutParams.WRAP_CONTENT
            height = WindowManager.LayoutParams.WRAP_CONTENT
            type = WindowManager.LayoutParams.TYPE_APPLICATION
            flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
            format = PixelFormat.TRANSLUCENT
            gravity = Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL
            x = 0
            y = 200
        }

        try {
            windowManager.addView(overlayView, layoutParams)
            isCreated = true
            Log.d(TAG, "Xposed archive overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create Xposed archive overlay", e)
            isCreated = false
        }
    }

    private fun createUI(activity: Activity) {
        val density = activity.resources.displayMetrics.density

        // Title
        titleText = TextView(activity).apply {
            text = "Archive Progress"
            setTextColor(Color.WHITE)
            textSize = 12f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (8f * density).toInt()
            }
        }
        contentContainer.addView(titleText)

        // Time display row
        val timeRow = LinearLayout(activity).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                progressBarWidth,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
        }

        // Current time (start)
        currentTimeText = TextView(activity).apply {
            text = "00:00"
            setTextColor(Color.WHITE)
            textSize = 11f
            layoutParams = LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1f
            )
        }
        timeRow.addView(currentTimeText)

        // Center time display container
        val centerTimeContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            )
            gravity = Gravity.CENTER_HORIZONTAL
        }

        // Seek time (shown during dragging)
        seekTimeText = TextView(activity).apply {
            text = ""
            setTextColor(Color.YELLOW)
            textSize = 11f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            visibility = View.GONE
            gravity = Gravity.CENTER
        }
        centerTimeContainer.addView(seekTimeText)

        // Last position text
        lastPositionText = TextView(activity).apply {
            text = ""
            setTextColor(Color.CYAN)
            textSize = 10f
            visibility = View.GONE
            gravity = Gravity.CENTER
        }
        centerTimeContainer.addView(lastPositionText)

        timeRow.addView(centerTimeContainer)

        // Total time (end)
        totalTimeText = TextView(activity).apply {
            text = "00:00"
            setTextColor(Color.WHITE)
            textSize = 11f
            layoutParams = LinearLayout.LayoutParams(
                0,
                LinearLayout.LayoutParams.WRAP_CONTENT,
                1f
            )
            gravity = Gravity.END
        }
        timeRow.addView(totalTimeText)

        contentContainer.addView(timeRow)

        // Spacer
        val spacer = View(activity).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                (8f * density).toInt()
            )
        }
        contentContainer.addView(spacer)

        // Progress bar container
        createProgressBar(activity)
        contentContainer.addView(progressContainer)
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createProgressBar(activity: Activity) {
        val density = activity.resources.displayMetrics.density

        progressContainer = FrameLayout(activity).apply {
            layoutParams = LinearLayout.LayoutParams(
                progressBarWidth,
                progressBarHeight
            )
        }

        // Background track
        val backgroundTrack = View(activity).apply {
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
            background = createRoundedBackground(Color.parseColor("#4DCCCCCC"), 12f * density)
        }
        progressContainer.addView(backgroundTrack)

        // Progress track (using custom view instead of system ProgressBar)
        progressBar = View(activity).apply {
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
            background = createRoundedBackground(Color.BLUE, 12f * density)
        }
        progressContainer.addView(progressBar)

        // Thumb
        progressThumb = View(activity).apply {
            layoutParams = FrameLayout.LayoutParams(
                thumbSize,
                thumbSize,
                Gravity.CENTER_VERTICAL or Gravity.START
            ).apply {
                leftMargin = 0 // Initial position
            }
            background = createRoundedBackground(Color.WHITE, 12f * density)
            elevation = 4f * density // Ensure thumb is above progress bar
        }
        progressContainer.addView(progressThumb)

        // Touch handling for progress bar
        progressContainer.setOnTouchListener { _, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    isDragging = true
                    val newPosition = (event.x / progressContainer.width).coerceIn(0f, 1f)
                    tempPosition = newPosition
                    Log.d(TAG, "ACTION_DOWN: x=${event.x}, width=${progressContainer.width}, position=$newPosition")
                    updateProgressUI()
                    true
                }
                MotionEvent.ACTION_MOVE -> {
                    if (isDragging) {
                        val newPosition = (event.x / progressContainer.width).coerceIn(0f, 1f)
                        tempPosition = newPosition
                        Log.v(TAG, "ACTION_MOVE: x=${event.x}, position=$newPosition")
                        updateProgressUI()
                    }
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    if (isDragging) {
                        Log.d(TAG, "ACTION_UP: position=$tempPosition, setting archive position")
                        isDragging = false
                        currentPosition = tempPosition
                        val seekSeconds = tempPosition * (archiveDuration / 1000f)
                        setArchivePosition(seekSeconds)
                        lastDraggedPosition = currentPosition
                        updateProgressUI()

                        // Auto-hide after setting position
                        hide()
                    }
                    true
                }
                else -> false
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

    @SuppressLint("ClickableViewAccessibility")
    private fun setupTouchHandling(activity: Activity) {
        overlayView.setOnTouchListener { view, event ->
            // Check if touch is in progress bar area (simplified check)
            val isInProgressBarArea = event.y > progressContainer.top && event.y < progressContainer.bottom && archiveDuration > 0

            if (isInProgressBarArea) {
                // Let progress bar handle the touch event
                false
            } else {
                // Handle window dragging
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
                        layoutParams.y = initialY - (event.rawY - initialTouchY).toInt()
                        windowManager.updateViewLayout(overlayView, layoutParams)
                        true
                    }
                    else -> false
                }
            }
        }
    }

    private fun setArchivePosition(seconds: Float) {
        try {
            // Call native function to set archive position
            LinkuraHookMain.setArchivePosition(seconds)
            Log.d(TAG, "Archive position set to: ${seconds}s via native call")
        } catch (e: Exception) {
            Log.e(TAG, "Error setting archive position", e)
        }
    }

    private fun updateUI() {
        Log.d(TAG, "updateUI called with archiveDuration: ${archiveDuration}ms")
        if (archiveDuration == 0L) {
            // Show message when no archive is running
            titleText.text = "No Archive Running"
            currentTimeText.visibility = View.GONE
            totalTimeText.visibility = View.GONE
            progressContainer.visibility = View.GONE
            seekTimeText.visibility = View.GONE
            lastPositionText.visibility = View.GONE
            Log.d(TAG, "No archive running - UI updated to show message")
        } else {
            // Show progress bar
            titleText.text = "Archive Progress"
            currentTimeText.visibility = View.VISIBLE
            totalTimeText.visibility = View.VISIBLE
            progressContainer.visibility = View.VISIBLE

            val durationSeconds = archiveDuration / 1000f
            val formattedDuration = formatTime(durationSeconds.toLong())

            currentTimeText.text = "00:00"
            totalTimeText.text = formattedDuration

            Log.d(TAG, "Archive progress UI updated - Duration: ${durationSeconds}s, Formatted: $formattedDuration")

            updateProgressUI()
        }
    }

    private fun updateProgressUI() {
        if (archiveDuration == 0L) return

        val displayPosition = if (isDragging) tempPosition else currentPosition
        val durationSeconds = archiveDuration / 1000f

        // Update progress bar width based on position
        val progressParams = progressBar.layoutParams as FrameLayout.LayoutParams
        progressParams.width = (progressBarWidth * displayPosition).toInt()
        progressBar.layoutParams = progressParams

        // Update progress bar color
        val progressColor = if (isDragging) Color.YELLOW else Color.BLUE
        val density = overlayView.context.resources.displayMetrics.density
        progressBar.background = createRoundedBackground(progressColor, 12f * density)

        // Update thumb position
        val thumbParams = progressThumb.layoutParams as FrameLayout.LayoutParams
        val maxOffset = progressBarWidth - thumbSize
        val newLeftMargin = (maxOffset * displayPosition).toInt()
        thumbParams.leftMargin = newLeftMargin
        progressThumb.layoutParams = thumbParams

        // Update thumb color and ensure visibility
        val thumbColor = if (isDragging) Color.YELLOW else Color.WHITE
        val cornerRadius = 12f * overlayView.context.resources.displayMetrics.density
        progressThumb.background = createRoundedBackground(thumbColor, cornerRadius)
        progressThumb.visibility = View.VISIBLE

        Log.d(TAG, "updateProgressUI: position=$displayPosition, leftMargin=$newLeftMargin, thumbColor=${if(isDragging) "YELLOW" else "WHITE"}, isDragging=$isDragging")

        // Update seek time text
        if (isDragging) {
            val seekSeconds = displayPosition * durationSeconds
            seekTimeText.text = formatTime(seekSeconds.toLong())
            seekTimeText.visibility = View.VISIBLE
            lastPositionText.visibility = View.GONE
        } else {
            seekTimeText.visibility = View.GONE
            if (lastDraggedPosition > 0f) {
                val lastSeconds = lastDraggedPosition * durationSeconds
                lastPositionText.text = "Last: ${formatTime(lastSeconds.toLong())}"
                lastPositionText.visibility = View.VISIBLE
            } else {
                lastPositionText.visibility = View.GONE
            }
        }
    }

    private fun formatTime(totalSeconds: Long): String {
        val hours = totalSeconds / 3600
        val minutes = (totalSeconds % 3600) / 60
        val seconds = totalSeconds % 60

        val formatted = if (hours > 0) {
            String.format("%d:%02d:%02d", hours, minutes, seconds)
        } else {
            String.format("%02d:%02d", minutes, seconds)
        }

        Log.d(TAG, "formatTime($totalSeconds) = $formatted (${hours}h ${minutes}m ${seconds}s)")
        return formatted
    }

    override fun isCreated(): Boolean = isCreated
    override fun isVisible(): Boolean = isVisible
}