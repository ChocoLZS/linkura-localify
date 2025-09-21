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
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import io.github.chocolzs.linkura.localify.LinkuraHookMain
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import kotlinx.coroutines.*
import kotlinx.serialization.Serializable

class XposedCameraInfoOverlayUI : BaseOverlay {
    override val overlayId = "camera_info"
    override val displayName = "Camera Info"

    override var onOverlayHidden: (() -> Unit)? = null
    override var onVisibilityChanged: ((Boolean) -> Unit)? = null

    private var isCreated = false
    private var isVisible = false
    private lateinit var windowManager: WindowManager
    private lateinit var overlayView: FrameLayout
    private lateinit var contentContainer: LinearLayout

    // UI Components
    private lateinit var titleText: TextView
    private lateinit var statusText: TextView
    private lateinit var positionText: TextView
    private lateinit var rotationText: TextView
    private lateinit var fovText: TextView
    private lateinit var modeText: TextView
    private lateinit var sceneText: TextView

    // Camera data
    private var cameraInfoState: CameraInfo? = null

    // Data update loop
    private var dataUpdateJob: Job? = null
    private val handler = Handler(Looper.getMainLooper())

    // State machine for toast notifications
    private var hasShownToast = false
    private var lastWasConnecting = false

    // Touch handling for window dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f

    @Serializable
    data class Vector3(val x: Float, val y: Float, val z: Float)

    @Serializable
    data class Quaternion(val x: Float, val y: Float, val z: Float, val w: Float)

    @Serializable
    data class CameraInfo(
        val isValid: Boolean,
        val position: Vector3,
        val rotation: Quaternion,
        val fov: Float,
        val mode: Int,
        val sceneType: Int,
        val isConnecting: Boolean = false
    )

    @SuppressLint("ClickableViewAccessibility")
    override fun show(context: Context) {
        if (isVisible) return

        val activity = context as? Activity ?: return

        try {
            createOverlay(activity)
            startDataUpdateLoop()
            isVisible = true
            onVisibilityChanged?.invoke(true)
            Log.d(TAG, "Xposed camera info overlay shown")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing Xposed camera info overlay", e)
        }
    }

    override fun hide() {
        if (!isVisible) return

        try {
            stopDataUpdateLoop()
            if (::overlayView.isInitialized) {
                windowManager.removeView(overlayView)
            }
            isVisible = false
            onVisibilityChanged?.invoke(false)
            onOverlayHidden?.invoke()
            Log.d(TAG, "Xposed camera info overlay hidden")
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding Xposed camera info overlay", e)
        }
    }

    override fun isCreated(): Boolean = isCreated
    override fun isVisible(): Boolean = isVisible

    @OptIn(DelicateCoroutinesApi::class)
    private fun startDataUpdateLoop() {
        dataUpdateJob = GlobalScope.launch {
            Log.d(TAG, "Starting camera data update loop")
            while (isActive && isVisible) {
                try {
                    // Get camera data from native
                    val protobufData = LinkuraHookMain.getCameraInfoProtobuf()
                    if (protobufData.isNotEmpty()) {
                        val cameraData = CameraData.parseFrom(protobufData)

                        // Update UI on main thread
                        handler.post {
                            updateCameraInfoFromProtobuf(cameraData)
                        }
                    } else {
                        // No data available
                        handler.post {
                            updateCameraInfoState(null)
                        }
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "Error in camera data update loop", e)
                }

                // Update at 10fps
                delay(100)
            }
            Log.d(TAG, "Camera data update loop stopped")
        }
    }

    private fun stopDataUpdateLoop() {
        dataUpdateJob?.cancel()
        dataUpdateJob = null
        Log.d(TAG, "Camera data update loop cancelled")
    }

    @SuppressLint("ClickableViewAccessibility")
    private fun createOverlay(activity: Activity) {
        windowManager = activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager

        val density = activity.resources.displayMetrics.density

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
                (120f * density).toInt(), // Fixed width to prevent text cutoff
                FrameLayout.LayoutParams.WRAP_CONTENT
            )

            val cornerRadius = 8f * density
            background = createRoundedBackground(Color.parseColor("#B3000000"), cornerRadius)

            val padding = (12f * density).toInt()
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
            gravity = Gravity.TOP or Gravity.START
            x = 100
            y = 200
        }

        try {
            windowManager.addView(overlayView, layoutParams)
            isCreated = true
            Log.d(TAG, "Xposed camera info overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create Xposed camera info overlay", e)
            isCreated = false
        }
    }

    private fun createUI(activity: Activity) {
        val density = activity.resources.displayMetrics.density

        // Title
        titleText = TextView(activity).apply {
            text = "Camera Info"
            setTextColor(Color.WHITE)
            textSize = 14f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (4f * density).toInt()
            }
        }
        contentContainer.addView(titleText)

        // Status
        statusText = TextView(activity).apply {
            text = "Connecting..."
            setTextColor(Color.GRAY)
            textSize = 10f
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                bottomMargin = (2f * density).toInt()
            }
        }
        contentContainer.addView(statusText)

        // Position
        positionText = TextView(activity).apply {
            text = "Pos: (0.00, 0.00, 0.00)"
            setTextColor(Color.WHITE)
            textSize = 10f
            visibility = View.GONE
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = (1f * density).toInt()
                bottomMargin = (1f * density).toInt()
            }
            maxLines = 1
            isSingleLine = true
            ellipsize = android.text.TextUtils.TruncateAt.END
        }
        contentContainer.addView(positionText)

        // Rotation
        rotationText = TextView(activity).apply {
            text = "Rot: (0.00, 0.00, 0.00, 0.00)"
            setTextColor(Color.WHITE)
            textSize = 10f
            visibility = View.GONE
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = (1f * density).toInt()
                bottomMargin = (1f * density).toInt()
            }
            maxLines = 1
            isSingleLine = true
            ellipsize = android.text.TextUtils.TruncateAt.END
        }
        contentContainer.addView(rotationText)

        // FOV
        fovText = TextView(activity).apply {
            text = "FOV: 0.0"
            setTextColor(Color.WHITE)
            textSize = 10f
            visibility = View.GONE
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = (1f * density).toInt()
                bottomMargin = (1f * density).toInt()
            }
        }
        contentContainer.addView(fovText)

        // Mode
        modeText = TextView(activity).apply {
            text = "Mode: UNKNOWN"
            setTextColor(Color.WHITE)
            textSize = 10f
            visibility = View.GONE
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = (1f * density).toInt()
                bottomMargin = (1f * density).toInt()
            }
        }
        contentContainer.addView(modeText)

        // Scene
        sceneText = TextView(activity).apply {
            text = "Scene: UNKNOWN"
            setTextColor(Color.WHITE)
            textSize = 10f
            visibility = View.GONE
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = (1f * density).toInt()
            }
        }
        contentContainer.addView(sceneText)
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

    private fun updateCameraInfoFromProtobuf(cameraData: CameraData) {
        try {
            val isConnecting = cameraData.hasIsConnecting() && cameraData.isConnecting

            val cameraInfo = CameraInfo(
                isValid = cameraData.isValid,
                position = Vector3(
                    x = cameraData.position.x,
                    y = cameraData.position.y,
                    z = cameraData.position.z
                ),
                rotation = Quaternion(
                    x = cameraData.rotation.x,
                    y = cameraData.rotation.y,
                    z = cameraData.rotation.z,
                    w = cameraData.rotation.w
                ),
                fov = cameraData.fov,
                mode = cameraData.mode,
                sceneType = cameraData.sceneType,
                isConnecting = isConnecting
            )

            // State machine logic for toast notifications
            handleStateTransition(cameraInfo)

            updateCameraInfoState(cameraInfo)
        } catch (e: Exception) {
            Log.e(TAG, "Error updating camera info from protobuf", e)
            updateCameraInfoState(null)
        }
    }

    private fun updateCameraInfoState(cameraInfo: CameraInfo?) {
        cameraInfoState = cameraInfo
        updateUI()
    }

    private fun updateUI() {
        if (!::statusText.isInitialized) return

        val info = cameraInfoState
        if (info == null) {
            // No data state
            statusText.text = "Connecting..."
            statusText.setTextColor(Color.GRAY)
            hideDetailTexts()
        } else if (info.isConnecting) {
            // Connecting state
            statusText.text = "Connecting..."
            statusText.setTextColor(Color.GRAY)
            hideDetailTexts()
        } else if (info.isValid) {
            // Valid data state
            statusText.visibility = View.GONE
            showDetailTexts()

            positionText.text = "Pos: ${String.format("%.1f,%.1f,%.1f", info.position.x, info.position.y, info.position.z)}"
            rotationText.text = "Rot: ${String.format("%.1f,%.1f,%.1f,%.1f", info.rotation.x, info.rotation.y, info.rotation.z, info.rotation.w)}"
            fovText.text = "FOV: ${String.format("%.1f", info.fov)}"
            modeText.text = "Mode: ${getModeString(info.mode)}"
            sceneText.text = "Scene: ${getSceneString(info.sceneType)}"
        } else {
            // Invalid data state
            statusText.text = "No Camera Data"
            statusText.setTextColor(Color.GRAY)
            statusText.visibility = View.VISIBLE
            hideDetailTexts()
        }
    }

    private fun hideDetailTexts() {
        positionText.visibility = View.GONE
        rotationText.visibility = View.GONE
        fovText.visibility = View.GONE
        modeText.visibility = View.GONE
        sceneText.visibility = View.GONE
    }

    private fun showDetailTexts() {
        positionText.visibility = View.VISIBLE
        rotationText.visibility = View.VISIBLE
        fovText.visibility = View.VISIBLE
        modeText.visibility = View.VISIBLE
        sceneText.visibility = View.VISIBLE
    }

    private fun handleStateTransition(cameraInfo: CameraInfo) {
        // Reset state when transitioning from connecting
        if (lastWasConnecting && !cameraInfo.isConnecting) {
            hasShownToast = false
            Log.d(TAG, "Reset toast state: transitioning from connecting")
        }

        // Check conditions for showing toast
        if (!hasShownToast &&
            cameraInfo.isValid &&
            !cameraInfo.isConnecting &&
            cameraInfo.sceneType == 2 && // WITH_LIVE = 2
            cameraInfo.mode != 0) { // mode != SYSTEM_CAMERA (0)

            showWarningToast()
            hasShownToast = true
            Log.d(TAG, "Warning toast shown for WITH_LIVE with non-SYSTEM_CAMERA mode")
        }

        lastWasConnecting = cameraInfo.isConnecting
    }

    private fun showWarningToast() {
        handler.post {
            val context = overlayView.context
            // Note: Using a generic warning message since we don't have access to R.string
            Toast.makeText(
                context,
                "Camera mode warning: This may cause crashes with live scenes",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    private fun getModeString(mode: Int): String {
        return when (mode) {
            0 -> "SYSTEM_CAMERA"
            1 -> "FREE"
            2 -> "FIRST_PERSON"
            3 -> "FOLLOW"
            else -> "UNKNOWN"
        }
    }

    private fun getSceneString(sceneType: Int): String {
        return when (sceneType) {
            0 -> "NONE"
            1 -> "FES_LIVE"
            2 -> "WITH_LIVE"
            3 -> "STORY"
            else -> "UNKNOWN"
        }
    }

    private fun createRoundedBackground(color: Int, cornerRadius: Float): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            setColor(color)
            setCornerRadius(cornerRadius)
        }
    }

    override fun destroy() {
        stopDataUpdateLoop()
        super.destroy()
    }
}