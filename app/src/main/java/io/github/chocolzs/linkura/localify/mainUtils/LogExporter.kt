package io.github.chocolzs.linkura.localify.mainUtils

import android.annotation.SuppressLint
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Environment
import android.provider.Settings
import android.util.Log
import java.io.BufferedReader
import java.io.File
import java.io.InputStreamReader
import java.text.SimpleDateFormat
import java.util.*

object LogExporter {
    private const val TAG = "LogExporter"
    
    // Broadcast action for log export notification
    const val ACTION_LOG_EXPORT_REQUEST = "io.github.chocolzs.linkura.localify.LOG_EXPORT_REQUEST"
    
    // In-memory log buffer for runtime logs
    private val logBuffer = mutableListOf<String>()
    private const val MAX_LOG_ENTRIES = 1000
    
    /**
     * Add a log entry to the internal buffer
     * This should be called from important parts of the app to capture runtime logs
     */
    fun addLogEntry(tag: String, level: String, message: String) {
        synchronized(logBuffer) {
            val timestamp = SimpleDateFormat("MM-dd HH:mm:ss.SSS", Locale.getDefault()).format(Date())
            val logEntry = "$timestamp $level/$tag: $message"
            
            logBuffer.add(logEntry)
            
            // Keep only the last MAX_LOG_ENTRIES
            if (logBuffer.size > MAX_LOG_ENTRIES) {
                logBuffer.removeAt(0)
            }
        }
    }
    
    /**
     * Export logs with basic Android information to Download folder
     * @param context Android context
     * @return File object if successful, null if failed
     */
    fun exportLogs(context: Context): File? {
        return try {
            val systemInfo = collectSystemInfo(context)
            val runtimeLogs = collectRuntimeLogs(context)
            val combinedContent = systemInfo + "\n" + runtimeLogs
            
            val timestamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(Date())
            val fileName = "linkura-localify-log-$timestamp.txt"
            
            val downloadsDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            val logFile = File(downloadsDir, fileName)
            
            logFile.writeText(combinedContent)
            Log.i(TAG, "Log exported successfully to: ${logFile.absolutePath}")
            
            // Send broadcast to notify Xposed side to export logs
            try {
                sendLogExportBroadcast(context, logFile.absolutePath)
                addLogEntry(TAG, "I", "Log export broadcast sent to Xposed side")
            } catch (e: Exception) {
                Log.w(TAG, "Failed to send log export broadcast", e)
                addLogEntry(TAG, "W", "Failed to send log export broadcast: ${e.message}")
            }
            
            logFile
        } catch (e: Exception) {
            Log.e(TAG, "Failed to export log", e)
            null
        }
    }
    
    /**
     * Send broadcast to notify Xposed side to export logs
     * @param context Android context
     * @param exportPath Path where the log was exported
     */
    private fun sendLogExportBroadcast(context: Context, exportPath: String) {
        val intent = Intent(ACTION_LOG_EXPORT_REQUEST)
        intent.putExtra("export_path", exportPath)
        // Use explicit package name to ensure broadcast reaches the target package
        intent.setPackage("com.oddno.lovelive")
        context.sendBroadcast(intent)
        Log.i(TAG, "Log export broadcast sent with path: $exportPath")
    }
    
    @SuppressLint("HardwareIds")
    private fun collectSystemInfo(context: Context): String {
        val sb = StringBuilder()
        val timestamp = SimpleDateFormat("yyyy-MM-dd_HH:mm:ss", Locale.getDefault()).format(Date())
        
        sb.appendLine("Linkura Localify Log Export")
        sb.appendLine("Generated: $timestamp")
        sb.appendLine("=".repeat(50))
        sb.appendLine()
        
        // Basic device information
        sb.appendLine("=== Device Information ===")
        sb.appendLine("Manufacturer: ${Build.MANUFACTURER}")
        sb.appendLine("Model: ${Build.MODEL}")
        sb.appendLine("Device: ${Build.DEVICE}")
        sb.appendLine("Product: ${Build.PRODUCT}")
        sb.appendLine("Brand: ${Build.BRAND}")
        sb.appendLine("Hardware: ${Build.HARDWARE}")
        sb.appendLine()
        
        // Android version information
        sb.appendLine("=== Android Version ===")
        sb.appendLine("Version Release: ${Build.VERSION.RELEASE}")
        sb.appendLine("Version SDK: ${Build.VERSION.SDK_INT}")
        sb.appendLine("Version Codename: ${Build.VERSION.CODENAME}")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            sb.appendLine("Version Base OS: ${Build.VERSION.BASE_OS}")
            sb.appendLine("Version Security Patch: ${Build.VERSION.SECURITY_PATCH}")
        }
        sb.appendLine()
        
        // App information
        sb.appendLine("=== App Information ===")
        try {
            val packageInfo = context.packageManager.getPackageInfo(context.packageName, 0)
            sb.appendLine("App Version: ${packageInfo.versionName}")
            sb.appendLine("Version Code: ${packageInfo.longVersionCode}")
            sb.appendLine("Package Name: ${packageInfo.packageName}")
            sb.appendLine("Target SDK: ${packageInfo.applicationInfo.targetSdkVersion}")
            sb.appendLine("Min SDK: ${packageInfo.applicationInfo.minSdkVersion}")
        } catch (e: PackageManager.NameNotFoundException) {
            sb.appendLine("Failed to get app information: ${e.message}")
        }
        sb.appendLine()
        
        // System settings (basic, non-sensitive)
        sb.appendLine("=== System Settings ===")
        try {
            val locale = Locale.getDefault()
            sb.appendLine("System Language: ${locale.language}")
            sb.appendLine("System Country: ${locale.country}")
            sb.appendLine("Display Language: ${locale.displayLanguage}")
            
            // Time zone
            val timeZone = TimeZone.getDefault()
            sb.appendLine("Time Zone: ${timeZone.id}")
            sb.appendLine("Time Zone Display: ${timeZone.displayName}")
        } catch (e: Exception) {
            sb.appendLine("Failed to get system settings: ${e.message}")
        }
        sb.appendLine()
        
        // Runtime information
        sb.appendLine("=== Runtime Information ===")
        val runtime = Runtime.getRuntime()
        sb.appendLine("Available Processors: ${runtime.availableProcessors()}")
        sb.appendLine("Max Memory: ${runtime.maxMemory() / 1024 / 1024} MB")
        sb.appendLine("Total Memory: ${runtime.totalMemory() / 1024 / 1024} MB")
        sb.appendLine("Free Memory: ${runtime.freeMemory() / 1024 / 1024} MB")
        sb.appendLine()
        
        // Storage information (basic)
        sb.appendLine("=== Storage Information ===")
        try {
            val internalStorage = Environment.getDataDirectory()
            sb.appendLine("Internal Storage Total: ${internalStorage.totalSpace / 1024 / 1024} MB")
            sb.appendLine("Internal Storage Free: ${internalStorage.freeSpace / 1024 / 1024} MB")
            
            if (Environment.getExternalStorageState() == Environment.MEDIA_MOUNTED) {
                val externalStorage = Environment.getExternalStorageDirectory()
                sb.appendLine("External Storage Total: ${externalStorage.totalSpace / 1024 / 1024} MB")
                sb.appendLine("External Storage Free: ${externalStorage.freeSpace / 1024 / 1024} MB")
            }
        } catch (e: Exception) {
            sb.appendLine("Failed to get storage information: ${e.message}")
        }
        sb.appendLine()
        
        // Permission information (basic app permissions only)
        sb.appendLine("=== App Permissions Status ===")
        val permissions = arrayOf(
            android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
            android.Manifest.permission.READ_EXTERNAL_STORAGE,
            android.Manifest.permission.SYSTEM_ALERT_WINDOW
        )
        
        for (permission in permissions) {
            val granted = context.checkSelfPermission(permission) == PackageManager.PERMISSION_GRANTED
            sb.appendLine("$permission: ${if (granted) "GRANTED" else "DENIED"}")
        }
        sb.appendLine()
        
        sb.appendLine("=== End of System Info ===")
        
        return sb.toString()
    }
    
    /**
     * Collect runtime logs from logcat
     * @param context Android context
     * @return Runtime logs as string
     */
    private fun collectRuntimeLogs(context: Context): String {
        val sb = StringBuilder()
        sb.appendLine()
        sb.appendLine("=== Runtime Logs ===")
        
        // First, add internal log buffer
        synchronized(logBuffer) {
            if (logBuffer.isNotEmpty()) {
                sb.appendLine()
                sb.appendLine("--- Internal Log Buffer (${logBuffer.size} entries) ---")
                logBuffer.forEach { logEntry ->
                    sb.appendLine(logEntry)
                }
            } else {
                sb.appendLine()
                sb.appendLine("--- Internal Log Buffer ---")
                sb.appendLine("No internal log entries captured yet.")
                sb.appendLine("Note: Internal logging needs to be implemented in app components.")
            }
        }
        
        // Then try to get logcat output
        sb.appendLine()
        sb.appendLine("--- System Logcat Output ---")
        
        try {
            // Get package name for filtering
            val packageName = context.packageName
            
            // Run logcat command to get recent logs for this app
            // Note: On Android 4.1+ (API 16+), apps can only read their own logs
            val process = Runtime.getRuntime().exec(arrayOf(
                "logcat", 
                "-d",           // dump and exit
                "-t", "500"     // last 500 lines
            ))
            
            val reader = BufferedReader(InputStreamReader(process.inputStream))
            var line: String?
            var logCount = 0
            val maxLines = 300  // Limit to last 300 lines to keep file size reasonable
            
            val allLines = mutableListOf<String>()
            while (reader.readLine().also { line = it } != null && logCount < 1000) {
                line?.let { 
                    // Filter logs that contain our app info or Linkura/Localify keywords
                    if (it.contains(packageName) || 
                        it.contains("LinkuraLocalify") || 
                        it.contains("LinkuraHookMain") ||
                        it.contains("LogExporter") ||
                        it.contains("Linkura") ||
                        it.contains("Localify") ||
                        it.contains("HasuKikaisann")) {  // native library name
                        allLines.add(it)
                    }
                }
                logCount++
            }
            
            // Take only the last maxLines to keep the log manageable
            val recentLines = if (allLines.size > maxLines) {
                allLines.takeLast(maxLines)
            } else {
                allLines
            }
            
            if (recentLines.isNotEmpty()) {
                sb.appendLine("Found ${recentLines.size} relevant logcat entries:")
                sb.appendLine()
                recentLines.forEach { logLine ->
                    sb.appendLine(logLine)
                }
            } else {
                sb.appendLine("No relevant logcat entries found.")
                sb.appendLine("This may be due to Android security restrictions.")
            }
            
            reader.close()
            process.waitFor()
            
        } catch (e: Exception) {
            sb.appendLine("Failed to collect logcat output: ${e.message}")
            sb.appendLine("This is expected on some Android versions due to security restrictions.")
            Log.w(TAG, "Failed to collect logcat output", e)
        }
        
        sb.appendLine()
        sb.appendLine("=== End of Runtime Logs ===")
        
        return sb.toString()
    }
}