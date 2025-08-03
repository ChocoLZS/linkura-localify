package io.github.chocolzs.linkura.localify.ui.overlay

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.Log

object OverlayManager {
    private const val TAG = "OverlayManager"
    private var isServiceRunning = false
    
    fun hasOverlayPermission(context: Context): Boolean {
        val hasPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            Settings.canDrawOverlays(context)
        } else {
            true
        }
        Log.d(TAG, "hasOverlayPermission: $hasPermission")
        return hasPermission
    }
    
    fun requestOverlayPermission(context: Context) {
        Log.d(TAG, "requestOverlayPermission called")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            try {
                val intent = Intent(
                    Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:${context.packageName}")
                )
                intent.flags = Intent.FLAG_ACTIVITY_NEW_TASK
                context.startActivity(intent)
                Log.d(TAG, "Permission request intent started")
            } catch (e: Exception) {
                Log.e(TAG, "Error requesting overlay permission", e)
            }
        }
    }
    
    fun startOverlay(context: Context): Boolean {
        Log.d(TAG, "startOverlay called with context: ${context.javaClass.simpleName}")

        if (!hasOverlayPermission(context)) {
            requestOverlayPermission(context)
            return false
        }
        
        if (!isServiceRunning) {
            try {
                val intent = Intent(context, OverlayService::class.java)
                
                val serviceResult = context.startService(intent)
                Log.d(TAG, "startService returned: $serviceResult")
                
                isServiceRunning = true
                Log.d(TAG, "OverlayService started successfully, marking as running")
                return true
            } catch (e: SecurityException) {
                Log.e(TAG, "SecurityException starting overlay service", e)
                return false
            } catch (e: Exception) {
                Log.e(TAG, "Error starting overlay service", e)
                return false
            }
        }
        Log.d(TAG, "Service already running, returning false")
        return false
    }
    
    @Deprecated("Use startOverlay() instead")
    fun startCameraOverlay(context: Context): Boolean = startOverlay(context)
    
    fun stopOverlay(context: Context) {
        Log.d(TAG, "stopOverlay called")
        if (isServiceRunning) {
            try {
                val intent = Intent(context, OverlayService::class.java)
                context.stopService(intent)
                isServiceRunning = false
            } catch (e: Exception) {
                Log.e(TAG, "Error stopping overlay service", e)
            }
        }
    }
    
    @Deprecated("Use stopOverlay() instead")
    fun stopCameraOverlay(context: Context) = stopOverlay(context)
    
    fun toggleOverlay(context: Context): Boolean {
        Log.d(TAG, "toggleOverlay called, current state: $isServiceRunning")
        return if (isServiceRunning) {
            stopOverlay(context)
            false
        } else {
            startOverlay(context)
        }
    }
    
    @Deprecated("Use toggleOverlay() instead")
    fun toggleCameraOverlay(context: Context): Boolean = toggleOverlay(context)
    
    fun isOverlayRunning(): Boolean = isServiceRunning
    
    fun markOverlayAsHidden() {
        // This method is called when the overlay is hidden but service is still running
        // We don't change isServiceRunning state as the service is still active
        Log.d(TAG, "Overlay marked as hidden (but service still running)")
    }
}