package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.app.Activity
import android.content.Context

interface BaseOverlay {
    /**
     * Unique identifier for this overlay
     */
    val overlayId: String

    /**
     * Display name for this overlay (used in buttons, logs, etc.)
     */
    val displayName: String

    /**
     * Show the overlay
     * @param context The context to create the overlay in
     */
    fun show(context: Context)

    /**
     * Hide the overlay
     */
    fun hide()

    /**
     * Check if the overlay is currently visible
     */
    fun isVisible(): Boolean

    /**
     * Check if the overlay has been created
     */
    fun isCreated(): Boolean

    /**
     * Callback when the overlay is hidden (for automatic state updates)
     */
    var onOverlayHidden: (() -> Unit)?

    /**
     * Callback when the overlay visibility changes
     */
    var onVisibilityChanged: ((Boolean) -> Unit)?

    /**
     * Optional: Clean up resources when the overlay is destroyed
     */
    fun destroy() {
        hide()
        onOverlayHidden = null
        onVisibilityChanged = null
    }
}