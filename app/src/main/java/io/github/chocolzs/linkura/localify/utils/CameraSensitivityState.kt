package io.github.chocolzs.linkura.localify.utils

import android.content.Context
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.ui.overlay.CameraSensitivityOverlayService

/**
 * Global state manager for camera sensitivity values
 * Provides reactive state synchronization between main app and overlay
 */
object CameraSensitivityState {
    private const val TAG = "CameraSensitivityState"
    private const val PREFS_NAME = "camera_sensitivity"
    
    // Reactive state values
    var movementSensitivity by mutableStateOf(1.0f)
        private set
    var verticalSensitivity by mutableStateOf(1.0f)
        private set
    var fovSensitivity by mutableStateOf(1.0f)
        private set
    var rotationSensitivity by mutableStateOf(1.0f)
        private set
    
    // Callbacks for external listeners
    private val changeListeners = mutableSetOf<() -> Unit>()
    private var overlayService: CameraSensitivityOverlayService? = null
    private var mainActivity: android.app.Activity? = null
    
    fun initialize(context: Context) {
        loadFromSharedPreferences(context)
        if (context is android.app.Activity) {
            mainActivity = context
        }
        Log.d(TAG, "CameraSensitivityState initialized")
    }
    
    fun setOverlayService(service: CameraSensitivityOverlayService?) {
        overlayService = service
        Log.d(TAG, "Overlay service reference ${if (service != null) "set" else "cleared"}")
    }
    
    private fun loadFromSharedPreferences(context: Context) {
        try {
            val sharedPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            movementSensitivity = sharedPrefs.getFloat("movement", 1.0f)
            verticalSensitivity = sharedPrefs.getFloat("vertical", 1.0f)
            fovSensitivity = sharedPrefs.getFloat("fov", 1.0f)
            rotationSensitivity = sharedPrefs.getFloat("rotation", 1.0f)
            Log.d(TAG, "Loaded sensitivity values: movement=$movementSensitivity, vertical=$verticalSensitivity, fov=$fovSensitivity, rotation=$rotationSensitivity")
        } catch (e: Exception) {
            Log.e(TAG, "Error loading sensitivity values, using defaults", e)
        }
    }
    
    private fun saveToSharedPreferences(context: Context) {
        try {
            val sharedPrefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            with(sharedPrefs.edit()) {
                putFloat("movement", movementSensitivity)
                putFloat("vertical", verticalSensitivity)
                putFloat("fov", fovSensitivity)
                putFloat("rotation", rotationSensitivity)
                apply()
            }
            Log.v(TAG, "Saved sensitivity values to SharedPreferences")
        } catch (e: Exception) {
            Log.e(TAG, "Error saving sensitivity values", e)
        }
    }
    
    /**
     * Update movement sensitivity
     * This will trigger reactive updates in both main app and overlay
     */
    fun updateMovementSensitivity(context: Context, value: Float) {
        if (movementSensitivity != value) {
            movementSensitivity = value
            saveToSharedPreferences(context)
            updateMainActivityConfig()
            notifyListeners()
            updateOverlayIfVisible()
            Log.d(TAG, "Movement sensitivity updated to: $value")
        }
    }
    
    /**
     * Update vertical sensitivity
     */
    fun updateVerticalSensitivity(context: Context, value: Float) {
        if (verticalSensitivity != value) {
            verticalSensitivity = value
            saveToSharedPreferences(context)
            updateMainActivityConfig()
            notifyListeners()
            updateOverlayIfVisible()
            Log.d(TAG, "Vertical sensitivity updated to: $value")
        }
    }
    
    /**
     * Update FOV sensitivity
     */
    fun updateFovSensitivity(context: Context, value: Float) {
        if (fovSensitivity != value) {
            fovSensitivity = value
            saveToSharedPreferences(context)
            updateMainActivityConfig()
            notifyListeners()
            updateOverlayIfVisible()
            Log.d(TAG, "FOV sensitivity updated to: $value")
        }
    }
    
    /**
     * Update rotation sensitivity
     */
    fun updateRotationSensitivity(context: Context, value: Float) {
        if (rotationSensitivity != value) {
            rotationSensitivity = value
            saveToSharedPreferences(context)
            updateMainActivityConfig()
            notifyListeners()
            updateOverlayIfVisible()
            Log.d(TAG, "Rotation sensitivity updated to: $value")
        }
    }
    
    /**
     * Update all sensitivity values at once
     * Used when receiving updates from overlay
     */
    fun updateAllSensitivity(
        context: Context,
        movement: Float,
        vertical: Float,
        fov: Float,
        rotation: Float
    ) {
        var changed = false
        
        if (movementSensitivity != movement) {
            movementSensitivity = movement
            changed = true
        }
        
        if (verticalSensitivity != vertical) {
            verticalSensitivity = vertical
            changed = true
        }
        
        if (fovSensitivity != fov) {
            fovSensitivity = fov
            changed = true
        }
        
        if (rotationSensitivity != rotation) {
            rotationSensitivity = rotation
            changed = true
        }
        
        if (changed) {
            saveToSharedPreferences(context)
            updateMainActivityConfig()
            notifyListeners()
            Log.d(TAG, "All sensitivity values updated: movement=$movement, vertical=$vertical, fov=$fov, rotation=$rotation")
        }
    }
    
    /**
     * Reset all values to default (1.0f)
     */
    fun resetToDefaults(context: Context) {
        updateAllSensitivity(context, 1.0f, 1.0f, 1.0f, 1.0f)
        updateOverlayIfVisible()
        Log.d(TAG, "All sensitivity values reset to defaults")
    }
    
    private fun updateMainActivityConfig() {
        try {
            // Import MainActivity class to access its config update methods
            val mainActivityClass = Class.forName("io.github.chocolzs.linkura.localify.MainActivity")
            val activity = mainActivity
            if (activity != null && mainActivityClass.isInstance(activity)) {
                // Use reflection to call the sensitivity update methods
                val movementMethod = mainActivityClass.getMethod("onCameraMovementSensitivityChanged", Float::class.java)
                val verticalMethod = mainActivityClass.getMethod("onCameraVerticalSensitivityChanged", Float::class.java)
                val fovMethod = mainActivityClass.getMethod("onCameraFovSensitivityChanged", Float::class.java)
                val rotationMethod = mainActivityClass.getMethod("onCameraRotationSensitivityChanged", Float::class.java)
                
                movementMethod.invoke(activity, movementSensitivity)
                verticalMethod.invoke(activity, verticalSensitivity)
                fovMethod.invoke(activity, fovSensitivity)
                rotationMethod.invoke(activity, rotationSensitivity)
                
                Log.v(TAG, "Updated MainActivity config via reflection")
            }
        } catch (e: Exception) {
            Log.v(TAG, "Could not update MainActivity config (activity not available): ${e.message}")
        }
    }
    
    /**
     * Get current values as a data class for easy access
     */
    data class SensitivityValues(
        val movement: Float,
        val vertical: Float,
        val fov: Float,
        val rotation: Float
    )
    
    fun getCurrentValues(): SensitivityValues {
        return SensitivityValues(
            movement = movementSensitivity,
            vertical = verticalSensitivity,
            fov = fovSensitivity,
            rotation = rotationSensitivity
        )
    }
    
    /**
     * Add a listener for sensitivity changes
     * Used by UI components that need to react to changes
     */
    fun addChangeListener(listener: () -> Unit) {
        changeListeners.add(listener)
        Log.v(TAG, "Change listener added, total: ${changeListeners.size}")
    }
    
    /**
     * Remove a change listener
     */
    fun removeChangeListener(listener: () -> Unit) {
        changeListeners.remove(listener)
        Log.v(TAG, "Change listener removed, total: ${changeListeners.size}")
    }
    
    private fun notifyListeners() {
        changeListeners.forEach { listener ->
            try {
                listener()
            } catch (e: Exception) {
                Log.e(TAG, "Error notifying change listener", e)
            }
        }
        Log.v(TAG, "Notified ${changeListeners.size} change listeners")
    }
    
    private fun updateOverlayIfVisible() {
        overlayService?.updateSensitivityValues(
            movementSensitivity,
            verticalSensitivity,
            fovSensitivity,
            rotationSensitivity
        )
    }
    
    /**
     * Send sensitivity update to native layer via AIDL
     */
    fun sendToNativeLayer(context: Context, sendMessageCallback: ((MessageType, com.google.protobuf.MessageLite) -> Boolean)? = null) {
        try {
            val configUpdate = ConfigUpdate.newBuilder()
                .setUpdateType(ConfigUpdateType.PARTIAL_UPDATE)
                .setCameraMovementSensitivity(movementSensitivity)
                .setCameraVerticalSensitivity(verticalSensitivity)
                .setCameraFovSensitivity(fovSensitivity)
                .setCameraRotationSensitivity(rotationSensitivity)
                .build()
            
            sendMessageCallback?.let { callback ->
                if (callback(MessageType.CONFIG_UPDATE, configUpdate)) {
                    Log.d(TAG, "Sensitivity update sent to native layer")
                } else {
                    Log.w(TAG, "Failed to send sensitivity update to native layer")
                }
            } ?: run {
                Log.d(TAG, "No send callback available, sensitivity not sent to native layer")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending sensitivity update to native layer", e)
        }
    }
}