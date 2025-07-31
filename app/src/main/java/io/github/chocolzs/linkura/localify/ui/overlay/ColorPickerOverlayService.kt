package io.github.chocolzs.linkura.localify.ui.overlay

import android.graphics.PixelFormat
import android.os.Build
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.WindowManager
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.ComposeView
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.ui.components.ColorPicker
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme

class ColorPickerOverlayService(private val parentService: OverlayService) {
    companion object {
        private const val TAG = "ColorPickerOverlay"
    }

    private var overlayView: View? = null
    private var isVisible by mutableStateOf(false)

    fun show(initialColor: Color) {
        if (isVisible) return
        createOverlay(initialColor)
        isVisible = true
    }

    fun hide() {
        if (!isVisible) return
        removeOverlay()
        isVisible = false
        parentService.resetColorPickerOverlayState()
    }

    private fun createOverlay(initialColor: Color) {
        val windowManager = parentService.getWindowManagerInstance() ?: return

        val layoutFlag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            WindowManager.LayoutParams.TYPE_PHONE
        }

        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.MATCH_PARENT,
            WindowManager.LayoutParams.MATCH_PARENT,
            layoutFlag,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
                    WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT
        )

        params.gravity = Gravity.CENTER

        overlayView = ComposeView(parentService).apply {
            setViewTreeLifecycleOwner(parentService.getLifecycleOwnerInstance())
            setViewTreeSavedStateRegistryOwner(parentService.getSavedStateRegistryOwnerInstance())
            setContent {
                LocalifyTheme {
                    ColorPickerOverlay(initialColor)
                }
            }
        }

        windowManager.addView(overlayView, params)
    }

    private fun removeOverlay() {
        overlayView?.let { parentService.getWindowManagerInstance()?.removeView(it) }
        overlayView = null
    }

    @Composable
    private fun ColorPickerOverlay(initialColor: Color) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.5f))
                .clickable { hide() },
            contentAlignment = Alignment.Center
        ) {
            Box(modifier = Modifier.clickable(enabled = false) {}) {
                ColorPicker(
                    onColorSelected = {
                        parentService.onColorChanged(it)
                        hide()
                    },
                    onDismiss = { hide() },
                    initialColor = initialColor
                )
            }
        }
    }

    fun destroy() {
        hide()
    }
}