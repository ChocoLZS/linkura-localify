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
    
    fun startCameraOverlay(context: Context): Boolean {
        Log.d(TAG, "startCameraOverlay called with context: ${context.javaClass.simpleName}")
        
        Log.d(TAG, "Checking overlay permission...")
        if (!hasOverlayPermission(context)) {
            Log.w(TAG, "No overlay permission, requesting...")
            requestOverlayPermission(context)
            return false
        }
        Log.d(TAG, "Overlay permission granted")
        
        if (!isServiceRunning) {
            try {
                Log.d(TAG, "Service not running, starting CameraOverlayService...")
                
                val intent = Intent(context, CameraOverlayService::class.java)
                Log.d(TAG, "Intent created: $intent")
                
                val serviceResult = context.startService(intent)
                Log.d(TAG, "startService returned: $serviceResult")
                
                isServiceRunning = true
                Log.d(TAG, "CameraOverlayService started successfully, marking as running")
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
    
    fun stopCameraOverlay(context: Context) {
        Log.d(TAG, "stopCameraOverlay called")
        if (isServiceRunning) {
            try {
                val intent = Intent(context, CameraOverlayService::class.java)
                context.stopService(intent)
                isServiceRunning = false
                Log.d(TAG, "CameraOverlayService stopped")
            } catch (e: Exception) {
                Log.e(TAG, "Error stopping overlay service", e)
            }
        }
    }
    
    fun toggleCameraOverlay(context: Context): Boolean {
        Log.d(TAG, "toggleCameraOverlay called, current state: $isServiceRunning")
        return if (isServiceRunning) {
            stopCameraOverlay(context)
            false
        } else {
            startCameraOverlay(context)
        }
    }
    
    fun isOverlayRunning(): Boolean = isServiceRunning
}