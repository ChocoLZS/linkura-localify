package io.github.chocolzs.linkura.localify.ui.overlay.xposed

import android.app.Activity
import android.content.Context
import android.util.Log
import android.widget.ImageButton
import io.github.chocolzs.linkura.localify.TAG

class OverlayManager {
    private val overlays = mutableMapOf<String, BaseOverlay>()
    private val buttonOverlayMap = mutableMapOf<ImageButton, String>()
    private val overlayButtonMap = mutableMapOf<String, ImageButton>()
    private var gameActivity: Activity? = null

    /**
     * Register an overlay with the manager
     */
    fun registerOverlay(overlay: BaseOverlay) {
        overlays[overlay.overlayId] = overlay

        // Set up the hidden callback to update button state
        overlay.onOverlayHidden = {
            updateButtonState(overlay.overlayId, false)
        }

        overlay.onVisibilityChanged = { isVisible ->
            updateButtonState(overlay.overlayId, isVisible)
        }

        Log.d(TAG, "Registered overlay: ${overlay.overlayId} (${overlay.displayName})")
    }

    /**
     * Unregister an overlay
     */
    fun unregisterOverlay(overlayId: String) {
        overlays[overlayId]?.destroy()
        overlays.remove(overlayId)

        // Clean up button mappings
        overlayButtonMap[overlayId]?.let { button ->
            buttonOverlayMap.remove(button)
        }
        overlayButtonMap.remove(overlayId)

        Log.d(TAG, "Unregistered overlay: $overlayId")
    }

    /**
     * Associate a button with an overlay
     */
    fun bindButton(button: ImageButton, overlayId: String, buttonUpdater: (ImageButton, Boolean) -> Unit) {
        if (!overlays.containsKey(overlayId)) {
            Log.w(TAG, "Cannot bind button to unknown overlay: $overlayId")
            return
        }

        buttonOverlayMap[button] = overlayId
        overlayButtonMap[overlayId] = button

        // Set up click listener
        button.setOnClickListener {
            toggleOverlay(overlayId, buttonUpdater)
        }

        Log.d(TAG, "Bound button to overlay: $overlayId")
    }

    /**
     * Toggle the visibility of an overlay
     */
    fun toggleOverlay(overlayId: String, buttonUpdater: ((ImageButton, Boolean) -> Unit)? = null) {
        val overlay = overlays[overlayId]
        if (overlay == null) {
            Log.w(TAG, "Cannot toggle unknown overlay: $overlayId")
            return
        }

        val activity = gameActivity
        if (activity == null) {
            Log.w(TAG, "Cannot toggle overlay $overlayId: no activity available")
            return
        }

        if (overlay.isVisible()) {
            Log.d(TAG, "Hiding overlay: $overlayId")
            overlay.hide()
            updateButtonState(overlayId, false, buttonUpdater)
        } else {
            Log.d(TAG, "Showing overlay: $overlayId")
            overlay.show(activity)
            updateButtonState(overlayId, true, buttonUpdater)
        }
    }

    /**
     * Show a specific overlay
     */
    fun showOverlay(overlayId: String) {
        val overlay = overlays[overlayId]
        val activity = gameActivity

        if (overlay == null) {
            Log.w(TAG, "Cannot show unknown overlay: $overlayId")
            return
        }

        if (activity == null) {
            Log.w(TAG, "Cannot show overlay $overlayId: no activity available")
            return
        }

        if (!overlay.isVisible()) {
            Log.d(TAG, "Showing overlay: $overlayId")
            overlay.show(activity)
            updateButtonState(overlayId, true)
        }
    }

    /**
     * Hide a specific overlay
     */
    fun hideOverlay(overlayId: String) {
        val overlay = overlays[overlayId]
        if (overlay == null) {
            Log.w(TAG, "Cannot hide unknown overlay: $overlayId")
            return
        }

        if (overlay.isVisible()) {
            Log.d(TAG, "Hiding overlay: $overlayId")
            overlay.hide()
            updateButtonState(overlayId, false)
        }
    }

    /**
     * Hide all visible overlays
     */
    fun hideAllOverlays() {
        overlays.values.forEach { overlay ->
            if (overlay.isVisible()) {
                overlay.hide()
                updateButtonState(overlay.overlayId, false)
            }
        }
        Log.d(TAG, "Hidden all overlays")
    }

    /**
     * Set the game activity for overlays to use
     */
    fun setGameActivity(activity: Activity) {
        gameActivity = activity
        Log.d(TAG, "Game activity set for overlay manager")
    }

    /**
     * Get overlay by ID
     */
    fun getOverlay(overlayId: String): BaseOverlay? = overlays[overlayId]

    /**
     * Get all registered overlay IDs
     */
    fun getRegisteredOverlayIds(): Set<String> = overlays.keys.toSet()

    /**
     * Check if an overlay is currently visible
     */
    fun isOverlayVisible(overlayId: String): Boolean {
        return overlays[overlayId]?.isVisible() ?: false
    }

    /**
     * Update button state for an overlay
     */
    private fun updateButtonState(
        overlayId: String,
        isVisible: Boolean,
        customUpdater: ((ImageButton, Boolean) -> Unit)? = null
    ) {
        val button = overlayButtonMap[overlayId]
        if (button != null) {
            if (customUpdater != null) {
                customUpdater(button, isVisible)
            }
            Log.d(TAG, "Updated button state for overlay $overlayId: visible=$isVisible")
        }
    }

    /**
     * Clean up all overlays and resources
     */
    fun destroy() {
        overlays.values.forEach { it.destroy() }
        overlays.clear()
        buttonOverlayMap.clear()
        overlayButtonMap.clear()
        gameActivity = null
        Log.d(TAG, "Overlay manager destroyed")
    }
}