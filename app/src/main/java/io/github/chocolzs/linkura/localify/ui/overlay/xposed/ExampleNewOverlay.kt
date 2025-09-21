package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.graphics.PixelFormat
import android.graphics.drawable.GradientDrawable
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import io.github.chocolzs.linkura.localify.TAG

/**
 * 示例：如何创建一个新的 overlay
 * 只需要实现 BaseOverlay 接口，然后在 OverlayToolbarUI.setupOverlays() 中注册即可
 */
class ExampleNewOverlay : BaseOverlay {
    override val overlayId = "example"
    override val displayName = "Example Overlay"

    override var onOverlayHidden: (() -> Unit)? = null
    override var onVisibilityChanged: ((Boolean) -> Unit)? = null

    private var isCreated = false
    private var isVisible = false
    private lateinit var windowManager: WindowManager
    private lateinit var overlayView: FrameLayout

    @SuppressLint("ClickableViewAccessibility")
    override fun show(context: Context) {
        if (isVisible) return

        val activity = context as? Activity ?: return

        try {
            createOverlay(activity)
            isVisible = true
            onVisibilityChanged?.invoke(true)
            Log.d(TAG, "Example overlay shown")
        } catch (e: Exception) {
            Log.e(TAG, "Error showing example overlay", e)
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
            Log.d(TAG, "Example overlay hidden")
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding example overlay", e)
        }
    }

    override fun isCreated(): Boolean = isCreated
    override fun isVisible(): Boolean = isVisible

    private fun createOverlay(activity: Activity) {
        windowManager = activity.getSystemService(Context.WINDOW_SERVICE) as WindowManager

        // Create main overlay container
        overlayView = FrameLayout(activity).apply {
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }

        // Create content
        val contentContainer = LinearLayout(activity).apply {
            orientation = LinearLayout.VERTICAL
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT
            )

            val cornerRadius = 12f * activity.resources.displayMetrics.density
            background = createRoundedBackground(Color.parseColor("#CC000000"), cornerRadius)

            val padding = (16f * activity.resources.displayMetrics.density).toInt()
            setPadding(padding, padding, padding, padding)
        }

        // Add title
        val titleText = TextView(activity).apply {
            text = "Example Overlay"
            setTextColor(Color.WHITE)
            textSize = 14f
            typeface = android.graphics.Typeface.DEFAULT_BOLD
        }
        contentContainer.addView(titleText)

        // Add description
        val descText = TextView(activity).apply {
            text = "This is an example of how easy it is to add new overlays!"
            setTextColor(Color.WHITE)
            textSize = 12f
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                topMargin = (8f * activity.resources.displayMetrics.density).toInt()
            }
        }
        contentContainer.addView(descText)

        overlayView.addView(contentContainer)

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
            Log.d(TAG, "Example overlay created successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to create example overlay", e)
            isCreated = false
        }
    }

    private fun createRoundedBackground(color: Int, cornerRadius: Float): GradientDrawable {
        return GradientDrawable().apply {
            shape = GradientDrawable.RECTANGLE
            setColor(color)
            setCornerRadius(cornerRadius)
        }
    }
}