package io.github.chocolzs.linkura.localify

import android.annotation.SuppressLint
import android.app.Activity
import android.app.AlertDialog
import android.app.AndroidAppHelper
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.Intent.FLAG_ACTIVITY_NEW_TASK
import android.content.IntentFilter
import android.net.Uri
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.KeyEvent
import android.view.MotionEvent
import android.widget.Toast
import com.bytedance.shadowhook.ShadowHook
import com.bytedance.shadowhook.ShadowHook.ConfigBuilder
import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.IXposedHookZygoteInit
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XSharedPreferences
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.XposedHelpers
import de.robv.android.xposed.callbacks.XC_LoadPackage
import io.github.chocolzs.linkura.localify.hookUtils.FilesChecker
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.mainUtils.AssetsRepository
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File
import java.util.Locale
import kotlin.system.measureTimeMillis
import io.github.chocolzs.linkura.localify.hookUtils.FileHotUpdater
import io.github.chocolzs.linkura.localify.hookUtils.FilesChecker.localizationFilesDir
import io.github.chocolzs.linkura.localify.mainUtils.LogExporter
import io.github.chocolzs.linkura.localify.mainUtils.json
import io.github.chocolzs.linkura.localify.models.NativeInitProgress
import io.github.chocolzs.linkura.localify.models.ProgramConfig
import io.github.chocolzs.linkura.localify.ui.game_attach.InitProgressUI
import io.github.chocolzs.linkura.localify.ui.overlay.xposed.OverlayToolbarUI

import io.github.chocolzs.linkura.localify.ipc.LinkuraAidlClient
import io.github.chocolzs.linkura.localify.ipc.MessageRouter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*

val TAG = "LinkuraLocalify"

class LinkuraHookMain : IXposedHookLoadPackage, IXposedHookZygoteInit  {
    private lateinit var modulePath: String
    private var nativeLibLoadSuccess: Boolean
    private var alreadyInitialized = false
    private val targetPackageName = "com.oddno.lovelive"
    private val nativeLibName = "HasuKikaisann"

    private var l4DataInited = false
    private var linkuraConfig: LinkuraConfig? = null

    private var getConfigError: Exception? = null
    private var externalFilesChecked: Boolean = false
    private var gameActivity: Activity? = null

    private val aidlClient: LinkuraAidlClient by lazy { LinkuraAidlClient.getInstance() }
    private val messageRouter: MessageRouter by lazy { MessageRouter() }
    private var isCameraInfoOverlayEnabled = false
    
    // Loop control variables
    private val loopControlFlags = mutableMapOf<String, Boolean>()
    
    // Broadcast receiver for log export requests
    private var logExportReceiver: BroadcastReceiver? = null
    
    private val aidlClientHandler = object : LinkuraAidlClient.MessageHandler {
        override fun onMessageReceived(type: MessageType, payload: ByteArray) {
            messageRouter.routeMessage(type, payload)
        }
        
        override fun onConnected() {
            Log.i(TAG, "AIDL client connected to service")
            LogExporter.addLogEntry(TAG, "I", "AIDL client connected to service")
            
            // Additional connection success diagnosis
            Log.i(TAG, "=== Connection Success Details ===")
            val connectionTime = System.currentTimeMillis()
            Log.i(TAG, "Connection established at: $connectionTime")
            LogExporter.addLogEntry(TAG, "I", "Connection success at $connectionTime")
        }
        
        override fun onDisconnected() {
            Log.i(TAG, "AIDL client disconnected from service")
            LogExporter.addLogEntry(TAG, "I", "AIDL client disconnected from service")
            isCameraInfoOverlayEnabled = false
            
            // Additional disconnection diagnosis
            Log.w(TAG, "=== Disconnection Details ===")
            val disconnectionTime = System.currentTimeMillis()
            Log.w(TAG, "Disconnection occurred at: $disconnectionTime")
            LogExporter.addLogEntry(TAG, "W", "Disconnection at $disconnectionTime")
        }
        
        override fun onConnectionFailed() {
            Log.w(TAG, "AIDL client failed to connect to service")
            LogExporter.addLogEntry(TAG, "W", "AIDL client failed to connect to service")
            
            // Enhanced connection failure diagnosis
            Log.e(TAG, "=== Connection Failure Details ===")
            val failureTime = System.currentTimeMillis()
            Log.e(TAG, "Connection failure occurred at: $failureTime")
            Log.e(TAG, "Current process UID: ${android.os.Process.myUid()}")
            Log.e(TAG, "Current thread: ${Thread.currentThread().name}")
            
            // Try to get more details about the failure
            try {
                val context = AndroidAppHelper.currentApplication()?.applicationContext
                if (context != null) {
                    diagnosePotentialConnectionIssues(context)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error during failure diagnosis: ${e.message}")
                LogExporter.addLogEntry(TAG, "E", "Failure diagnosis error: ${e.message}")
            }
        }
    }
    
    private val configUpdateHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                updateConfig(payload)
                Log.i(TAG, "Config update sent to native layer")
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error processing config update", e)
                false
            }
        }
    }
    
    private val cameraInfoOverlayControlHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val overlayControl = OverlayControl.parseFrom(payload)
                when (overlayControl.action) {
                    OverlayAction.START_CAMERA_INFO_OVERLAY -> {
                        isCameraInfoOverlayEnabled = true
                        Log.i(TAG, "Camera info overlay enabled by control")
                    }
                    OverlayAction.STOP_CAMERA_INFO_OVERLAY -> {
                        isCameraInfoOverlayEnabled = false
                        Log.i(TAG, "Camera info overlay disabled by control")
                    }
                    else -> {
                        Log.w(TAG, "Unknown camera info overlay action: ${overlayControl.action}")
                        return false
                    }
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error processing camera info overlay control", e)
                false
            }
        }
    }
    
    private val archiveInfoHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val archiveInfoRequest = ArchiveInfo.parseFrom(payload)
                // Get archive info from native layer
                val archiveInfoBytes = getCurrentArchiveInfo()

                if (archiveInfoBytes.isNotEmpty()) {
                    // Parse the archive info from native layer
                    val nativeArchiveInfo = ArchiveInfo.parseFrom(archiveInfoBytes)


                    if (aidlClient.isClientConnected()) {
                        val success = aidlClient.sendMessage(MessageType.ARCHIVE_INFO, nativeArchiveInfo)
                        if (success) {
                            Log.i(TAG, "Archive info sent: duration=${nativeArchiveInfo.duration}ms")
                        } else {
                            Log.w(TAG, "Failed to send archive info")
                        }
                    }
                } else {
                    // No archive info available, send empty response with duration 0
                    val emptyResponse = ArchiveInfo.newBuilder()
                        .setDuration(0L)
                        .build()


                    if (aidlClient.isClientConnected()) {
                        val success = aidlClient.sendMessage(MessageType.ARCHIVE_INFO, emptyResponse)
                        if (success) {
                            Log.i(TAG, "Archive info sent: no archive running (duration=0)")
                        } else {
                            Log.w(TAG, "Failed to send archive info")
                        }
                    }
                }
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error processing archive info request", e)
                false
            }
        }
    }
    
    private val archivePositionSetHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                Log.i(TAG, "Received archive position and ready to set position")
                val positionRequest = ArchivePositionSetRequest.parseFrom(payload)
                Log.i(TAG, "Received archive position: seconds=${positionRequest.seconds}")
                val seconds = positionRequest.seconds
                
                // Call native function to set archive position
                setArchivePosition(seconds)
                Log.i(TAG, "Archive position set to: ${seconds}s")
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error processing archive position set request", e)
                false
            }
        }
    }
    
    private val cameraBackgroundColorHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val colorMessage = CameraBackgroundColor.parseFrom(payload)
                Log.i(TAG, "Received camera background color: R=${colorMessage.red}, G=${colorMessage.green}, B=${colorMessage.blue}, A=${colorMessage.alpha}")

                setCameraBackgroundColor(colorMessage.red, colorMessage.green, colorMessage.blue, colorMessage.alpha)
                Log.i(TAG, "Camera background color updated successfully")
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error processing camera background color", e)
                false
            }
        }
    }
    
    private val virtualKeyboardInputHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val keyboardInput = VirtualKeyboardInput.parseFrom(payload)
                Log.v(TAG, "Received virtual keyboard input: keyCode=${keyboardInput.keyCode}, action=${keyboardInput.action}")

                // Forward to native keyboard event handler
                keyboardEvent(keyboardInput.keyCode, keyboardInput.action)
                
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error processing virtual keyboard input", e)
                false
            }
        }
    }
    
    private val virtualJoystickInputHandler = object : MessageRouter.MessageTypeHandler {
        override fun handleMessage(payload: ByteArray): Boolean {
            return try {
                val joystickInput = VirtualJoystickInput.parseFrom(payload)
                Log.v(TAG, "Received virtual joystick input: action=${joystickInput.action}, rightX=${joystickInput.rightStickX}, rightY=${joystickInput.rightStickY}")

                // Forward to native joystick event handler
                joystickEvent(
                    joystickInput.action,
                    joystickInput.leftStickX,
                    joystickInput.leftStickY,
                    joystickInput.rightStickX,
                    joystickInput.rightStickY,
                    joystickInput.leftTrigger,
                    joystickInput.rightTrigger,
                    joystickInput.hatX,
                    joystickInput.hatY
                )
                
                true
            } catch (e: Exception) {
                Log.e(TAG, "Error processing virtual joystick input", e)
                false
            }
        }
    }

    private fun onStartHandler() {
        aidlClient.sendMessage(MessageType.CAMERA_OVERLAY_REQUEST, CameraOverlayRequest.newBuilder().build());
    }

    
    /**
     * Setup broadcast receiver for log export requests
     */
    private fun setupLogExportBroadcastReceiver(context: Context) {
        try {
            if (logExportReceiver == null) {
                logExportReceiver = object : BroadcastReceiver() {
                    override fun onReceive(context: Context?, intent: Intent?) {
                        if (intent?.action == LogExporter.ACTION_LOG_EXPORT_REQUEST) {
                            val exportPath = intent.getStringExtra("export_path")
                            Log.i(TAG, "Received log export broadcast from Java side, export path: $exportPath")
                            LogExporter.addLogEntry(TAG, "I", "Received log export broadcast from Java side")
                            
                            try {
                                // Export Xposed side logs
                                val xposedLogFile = exportXposedLogs(context)
                                if (xposedLogFile != null) {
                                    Log.i(TAG, "Xposed logs exported successfully to: ${xposedLogFile.absolutePath}")
                                    LogExporter.addLogEntry(TAG, "I", "Xposed logs exported to: ${xposedLogFile.name}")
                                } else {
                                    Log.e(TAG, "Failed to export Xposed logs")
                                    LogExporter.addLogEntry(TAG, "E", "Failed to export Xposed logs")
                                }
                            } catch (e: Exception) {
                                Log.e(TAG, "Error exporting Xposed logs", e)
                                LogExporter.addLogEntry(TAG, "E", "Error exporting Xposed logs: ${e.message}")
                            }
                        }
                    }
                }
                
                val intentFilter = IntentFilter(LogExporter.ACTION_LOG_EXPORT_REQUEST)
                context.registerReceiver(logExportReceiver, intentFilter, Context.RECEIVER_EXPORTED)
                Log.i(TAG, "Log export broadcast receiver registered")
                LogExporter.addLogEntry(TAG, "I", "Log export broadcast receiver registered")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to setup log export broadcast receiver", e)
            LogExporter.addLogEntry(TAG, "E", "Failed to setup log export broadcast receiver: ${e.message}")
        }
    }
    
    /**
     * Export Xposed side logs
     */
    private fun exportXposedLogs(context: Context?): java.io.File? {
        return try {
            val timestamp = java.text.SimpleDateFormat("yyyyMMdd_HHmmss", java.util.Locale.getDefault()).format(java.util.Date())
            val fileName = "linkura-localify-xposed-log-$timestamp.txt"
            
            val downloadsDir = android.os.Environment.getExternalStoragePublicDirectory(android.os.Environment.DIRECTORY_DOWNLOADS)
            val logFile = java.io.File(downloadsDir, fileName)
            
            // Collect Xposed side log information
            val sb = StringBuilder()
            sb.appendLine("Linkura Localify Xposed Side Log Export")
            sb.appendLine("Generated: ${java.text.SimpleDateFormat("yyyy-MM-dd_HH:mm:ss", java.util.Locale.getDefault()).format(java.util.Date())}")
            sb.appendLine("=".repeat(50))
            sb.appendLine()
            
            // Native library status
            sb.appendLine("=== Native Library Status ===")
            sb.appendLine("Native Lib Load Success: $nativeLibLoadSuccess")
            sb.appendLine("Already Initialized: $alreadyInitialized")
            sb.appendLine("L4 Data Initialized: $l4DataInited")
            sb.appendLine("External Files Checked: $externalFilesChecked")
            sb.appendLine()
            
            // Socket client status
            sb.appendLine("=== Socket Client Status ===")
            sb.appendLine("AIDL Client Connected: ${aidlClient.isClientConnected()}")
            sb.appendLine("Camera Info Overlay Enabled: $isCameraInfoOverlayEnabled")
            sb.appendLine()
            
            // Loop control flags
            sb.appendLine("=== Loop Control Status ===")
            loopControlFlags.forEach { (name, enabled) ->
                sb.appendLine("$name: $enabled")
            }
            sb.appendLine()
            
            // Game activity status
            sb.appendLine("=== Game Activity Status ===")
            sb.appendLine("Game Activity Available: ${gameActivity != null}")
            sb.appendLine("Game Activity Class: ${gameActivity?.javaClass?.name ?: "null"}")
            sb.appendLine()
            
            // Module path and target info
            sb.appendLine("=== Module Information ===")
            sb.appendLine("Module Path: $modulePath")
            sb.appendLine("Target Package: $targetPackageName")
            sb.appendLine("Native Library Name: $nativeLibName")
            sb.appendLine()
            
            // Config error status
            sb.appendLine("=== Configuration Status ===")
            if (getConfigError != null) {
                sb.appendLine("Config Error: ${getConfigError.toString()}")
            } else {
                sb.appendLine("Config Error: None")
            }
            sb.appendLine()
            
            // Add LogExporter buffer content from Xposed side
            sb.appendLine("=== Xposed Side LogExporter Buffer ===")
            try {
                // Use reflection to access LogExporter's private logBuffer
                val logExporterClass = LogExporter::class.java
                val logBufferField = logExporterClass.getDeclaredField("logBuffer")
                logBufferField.isAccessible = true
                @Suppress("UNCHECKED_CAST")
                val logBuffer = logBufferField.get(null) as MutableList<String>
                
                synchronized(logBuffer) {
                    if (logBuffer.isNotEmpty()) {
                        sb.appendLine("LogExporter Buffer (${logBuffer.size} entries):")
                        logBuffer.forEach { logEntry ->
                            sb.appendLine(logEntry)
                        }
                    } else {
                        sb.appendLine("LogExporter Buffer is empty")
                    }
                }
            } catch (e: Exception) {
                sb.appendLine("Failed to access LogExporter buffer: ${e.message}")
                Log.w(TAG, "Failed to access LogExporter buffer", e)
            }
            sb.appendLine()
            
            sb.appendLine("=== End of Xposed Side Log ===")
            
            logFile.writeText(sb.toString())
            logFile
        } catch (e: Exception) {
            Log.e(TAG, "Failed to export Xposed logs", e)
            null
        }
    }

    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam) {
//        if (lpparam.packageName == "io.github.chocolzs.linkura.localify") {
//            XposedHelpers.findAndHookMethod(
//                "io.github.chocolzs.linkura.localify.MainActivity",
//                lpparam.classLoader,
//                "showToast",
//                String::class.java,
//                object : XC_MethodHook() {
//                    override fun beforeHookedMethod(param: MethodHookParam) {
//                        Log.d(TAG, "beforeHookedMethod hooked: ${param.args}")
//                    }
//                }
//            )
//        }

        if (lpparam.packageName != targetPackageName) {
            return
        }

        XposedHelpers.findAndHookMethod(
            "android.app.Activity",
            lpparam.classLoader,
            "dispatchKeyEvent",
            KeyEvent::class.java,
            object : XC_MethodHook() {
                override fun beforeHookedMethod(param: MethodHookParam) {
                    val keyEvent = param.args[0] as KeyEvent
                    val keyCode = keyEvent.keyCode
                    val action = keyEvent.action
                    // Log.d(TAG, "Key event: keyCode=$keyCode, action=$action")
                    keyboardEvent(keyCode, action)
                }
            }
        )

        XposedHelpers.findAndHookMethod(
            "android.app.Activity",
            lpparam.classLoader,
            "dispatchGenericMotionEvent",
            MotionEvent::class.java,
            object : XC_MethodHook() {
                override fun beforeHookedMethod(param: MethodHookParam) {
                    val motionEvent = param.args[0] as MotionEvent
                    val action = motionEvent.action

                    // 左摇杆的X和Y轴
                    val leftStickX = motionEvent.getAxisValue(MotionEvent.AXIS_X)
                    val leftStickY = motionEvent.getAxisValue(MotionEvent.AXIS_Y)

                    // 右摇杆的X和Y轴
                    val rightStickX = motionEvent.getAxisValue(MotionEvent.AXIS_Z)
                    val rightStickY = motionEvent.getAxisValue(MotionEvent.AXIS_RZ)

                    // 左扳机
                    val leftTrigger = motionEvent.getAxisValue(MotionEvent.AXIS_LTRIGGER)

                    // 右扳机
                    val rightTrigger = motionEvent.getAxisValue(MotionEvent.AXIS_RTRIGGER)

                    // 十字键
                    val hatX = motionEvent.getAxisValue(MotionEvent.AXIS_HAT_X)
                    val hatY = motionEvent.getAxisValue(MotionEvent.AXIS_HAT_Y)

                    // 处理摇杆和扳机事件
                    joystickEvent(
                        action,
                        leftStickX,
                        leftStickY,
                        rightStickX,
                        rightStickY,
                        leftTrigger,
                        rightTrigger,
                        hatX,
                        hatY
                    )
                }
            }
        )

        val appActivityClass = XposedHelpers.findClass("android.app.Activity", lpparam.classLoader)
        XposedBridge.hookAllMethods(appActivityClass, "onStart", object : XC_MethodHook() {
            override fun beforeHookedMethod(param: MethodHookParam) {
                super.beforeHookedMethod(param)
                Log.d(TAG, "onStart")
                val currActivity = param.thisObject as Activity
                gameActivity = currActivity
                if (getConfigError != null) {
                    showGetConfigFailed(currActivity)
                }
                else {
                    initLinkuraConfig(currActivity)
                }
                onStartHandler()
                // load config
                // Create overlay toolbar after initialization is complete
                if (!overlayToolbarUI.isOverlayCreated() && linkuraConfig?.enableInGameOverlayToolbar == true) {
                    Log.d(TAG, "Start overlay")
                    overlayToolbarUI.createOverlay(gameActivity!!)
                }
                // Setup log export broadcast receiver
                setupLogExportBroadcastReceiver(currActivity)
            }
        })

        XposedBridge.hookAllMethods(appActivityClass, "onResume", object : XC_MethodHook() {
            override fun beforeHookedMethod(param: MethodHookParam) {
                Log.d(TAG, "onResume")
                val currActivity = param.thisObject as Activity
                gameActivity = currActivity
                if (getConfigError != null) {
                    showGetConfigFailed(currActivity)
                }
                else {
                    initLinkuraConfig(currActivity)
                }
            }
        })

        val cls = lpparam.classLoader.loadClass("com.unity3d.player.UnityPlayer")
        XposedHelpers.findAndHookMethod(
            cls,
            "loadNative",
            String::class.java,
            object : XC_MethodHook() {
                @SuppressLint("UnsafeDynamicallyLoadedCode")
                override fun afterHookedMethod(param: MethodHookParam) {
                    super.afterHookedMethod(param)

                    Log.i(TAG, "UnityPlayer.loadNative")
                    LogExporter.addLogEntry(TAG, "I", "UnityPlayer.loadNative called")

                    if (alreadyInitialized) {
                        LogExporter.addLogEntry(TAG, "W", "Already initialized, skipping")
                        return
                    }

                    val app = AndroidAppHelper.currentApplication()
                    if (nativeLibLoadSuccess) {
                        showToast("lib$nativeLibName.so loaded.")
                        LogExporter.addLogEntry(TAG, "I", "Native library lib$nativeLibName.so loaded successfully")
                    }
                    else {
                        showToast("Load native library lib$nativeLibName.so failed.")
                        LogExporter.addLogEntry(TAG, "E", "Failed to load native library lib$nativeLibName.so")
                        return
                    }

                    if (!l4DataInited) {
                        requestConfig(app.applicationContext)
                    }

                    FilesChecker.initDir(app.filesDir, modulePath)
                    initHook(
                        "${app.applicationInfo.nativeLibraryDir}/libil2cpp.so",
                        File(
                            app.filesDir.absolutePath,
                            FilesChecker.localizationFilesDir
                        ).absolutePath
                    )

                    alreadyInitialized = true
                    LogExporter.addLogEntry(TAG, "I", "Hook initialization completed successfully")
                    
                    // Setup AIDL client for duplex communication
                    setupAidlClient(app.applicationContext)
                }
            })

        // Start main loop at 30fps
        startCustomLoop("MainLoop", 30) {
            executeMainLoopTasks()
        }

        // Start camera data loop at 10fps
        startCustomLoop("CameraDataLoop", 10) {
            executeCameraDataTasks()
        }
    }


    // Main loop tasks - runs at 30fps
    private var lastFrameStartInit = NativeInitProgress.startInit
    private val initProgressUI = InitProgressUI()
    private val overlayToolbarUI = OverlayToolbarUI()
    
    private fun executeMainLoopTasks() {
        val returnValue = pluginCallbackLooper()  // plugin main thread loop
        if (returnValue == 9) {
            NativeInitProgress.startInit = true
        }

        if (NativeInitProgress.startInit) {  // if init, update data
            NativeInitProgress.pluginInitProgressLooper(NativeInitProgress)
            gameActivity?.let { initProgressUI.updateData(it) }
        }

        if ((gameActivity != null) && (lastFrameStartInit != NativeInitProgress.startInit)) {  // change status
            if (NativeInitProgress.startInit) {
                initProgressUI.createView(gameActivity!!)
            }
            else {
                initProgressUI.finishLoad(gameActivity!!)
            }
        }
        lastFrameStartInit = NativeInitProgress.startInit
    }
    
    // Camera data tasks - runs at 30fps
    private fun executeCameraDataTasks() {
        if (isCameraInfoOverlayEnabled && !sharedIgnoreCameraInfoLoop) {
            try {
                Log.v(TAG, "Trying to get camera info protobuf")
                val protobufData = getCameraInfoProtobuf()
                if (protobufData.isNotEmpty()) {
                    val cameraData = CameraData.parseFrom(protobufData)
                    if (aidlClient.isClientConnected()) {
                        Log.v(TAG, "Sending camera data")
                        val success = aidlClient.sendMessage(MessageType.CAMERA_DATA, cameraData)
                        if (!success) {
                            Log.w(TAG, "Failed to send camera data via socket")
                        }
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in camera data loop", e)
            }
        }
    }
    
    // Generic loop starter with custom frequency and control
    @OptIn(DelicateCoroutinesApi::class)
    private fun startCustomLoop(name: String, fps: Int, task: () -> Unit) {
        // Initialize control flag
        loopControlFlags[name] = true
        
        GlobalScope.launch {
            val interval = 1000L / fps
            Log.d(TAG, "Starting $name loop at ${fps}fps (interval: ${interval}ms)")
            
            var loopCounter = 0L
            
            while (isActive && (loopControlFlags[name] == true)) {
                val timeTaken = measureTimeMillis {
                    try {
                        // Only execute task if loop is enabled
                        if (loopControlFlags[name] == true) {
                            task()
                            loopCounter++
                            
                            // Log performance every 100 iterations for debugging
//                            if (loopCounter % 100 == 0L) {
//                                Log.v(TAG, "$name loop: ${loopCounter} iterations completed")
//                            }
                        }
                    } catch (e: Exception) {
                        Log.e(TAG, "Error in $name loop (iteration $loopCounter)", e)
                    }
                }
                
                // Ensure we don't have negative delay
                val delayTime = maxOf(1L, interval - timeTaken)
                
                // Warn if loop is running too slow
                if (timeTaken > interval && loopCounter % 10 == 0L) {
                    Log.w(TAG, "$name loop is running slow: took ${timeTaken}ms, expected ${interval}ms")
                }
                
                delay(delayTime)
            }
            
            Log.d(TAG, "$name loop terminated after $loopCounter iterations")
        }
    }

    private fun setupAidlClient(context: Context) {
        Log.i(TAG, "=== AIDL Client Setup Diagnosis ===")
        LogExporter.addLogEntry(TAG, "I", "Starting AIDL client setup diagnosis")
        
        // Device info diagnosis
        Log.i(TAG, "Android API level: ${Build.VERSION.SDK_INT}")
        Log.i(TAG, "Android version: ${Build.VERSION.RELEASE}")
        Log.i(TAG, "Device manufacturer: ${Build.MANUFACTURER}")
        Log.i(TAG, "Device model: ${Build.MODEL}")
//        LogExporter.addLogEntry(TAG, "I", "Device: API${Build.VERSION.SDK_INT}, Root=$isRooted, ${Build.MANUFACTURER} ${Build.MODEL}")
        
        // Target package diagnosis
        try {
            val packageInfo = context.packageManager.getPackageInfo("io.github.chocolzs.linkura.localify", 0)
            Log.i(TAG, "Target package found: version=${packageInfo.versionName}, code=${packageInfo.longVersionCode}")
            LogExporter.addLogEntry(TAG, "I", "Target package: ${packageInfo.versionName} (${packageInfo.longVersionCode})")
            
            val appInfo = packageInfo.applicationInfo
            Log.i(TAG, "Target package enabled: ${appInfo.enabled}")
            Log.i(TAG, "Target package system app: ${(appInfo.flags and android.content.pm.ApplicationInfo.FLAG_SYSTEM) != 0}")
            Log.i(TAG, "Target package uid: ${appInfo.uid}")
            LogExporter.addLogEntry(TAG, "I", "Target app: enabled=${appInfo.enabled}, uid=${appInfo.uid}")
        } catch (e: Exception) {
            Log.e(TAG, "Target package not found or error: ${e.message}")
            LogExporter.addLogEntry(TAG, "E", "Target package error: ${e.message}")
        }
        
        // Current app info
        val currentAppInfo = context.applicationInfo
        Log.i(TAG, "Current app uid: ${currentAppInfo.uid}")
        Log.i(TAG, "Current app process name: ${android.os.Process.myPid()}")
        LogExporter.addLogEntry(TAG, "I", "Current app: uid=${currentAppInfo.uid}, pid=${android.os.Process.myPid()}")
        
        // Service intent diagnosis
        val serviceIntent = android.content.Intent().apply {
            setClassName("io.github.chocolzs.linkura.localify", "io.github.chocolzs.linkura.localify.ipc.LinkuraAidlService")
        }
        
        try {
            val resolveInfo = context.packageManager.resolveService(serviceIntent, 0)
            if (resolveInfo != null) {
                Log.i(TAG, "AIDL service found: ${resolveInfo.serviceInfo.name}")
                Log.i(TAG, "Service enabled: ${resolveInfo.serviceInfo.enabled}")
                Log.i(TAG, "Service exported: ${resolveInfo.serviceInfo.exported}")
                Log.i(TAG, "Service permission: ${resolveInfo.serviceInfo.permission ?: "none"}")
                LogExporter.addLogEntry(TAG, "I", "AIDL service: enabled=${resolveInfo.serviceInfo.enabled}, exported=${resolveInfo.serviceInfo.exported}")
            } else {
                Log.w(TAG, "AIDL service not found or not resolvable")
                LogExporter.addLogEntry(TAG, "W", "AIDL service not found")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error resolving AIDL service: ${e.message}")
            LogExporter.addLogEntry(TAG, "E", "Service resolve error: ${e.message}")
        }
        
        // // Permission diagnosis
        // val permissions = arrayOf(
        //     "android.permission.BIND_DEVICE_ADMIN",
        //     "android.permission.SYSTEM_ALERT_WINDOW",
        //     "android.permission.QUERY_ALL_PACKAGES"
        // )
        
        // permissions.forEach { permission ->
        //     val granted = context.checkSelfPermission(permission) == android.content.pm.PackageManager.PERMISSION_GRANTED
        //     Log.i(TAG, "Permission $permission: $granted")
        // }
        
        // Register message handlers
        messageRouter.registerHandler(MessageType.CONFIG_UPDATE, configUpdateHandler)
        messageRouter.registerHandler(MessageType.OVERLAY_CONTROL_CAMERA_INFO, cameraInfoOverlayControlHandler)
        messageRouter.registerHandler(MessageType.ARCHIVE_INFO, archiveInfoHandler)
        messageRouter.registerHandler(MessageType.ARCHIVE_POSITION_SET_REQUEST, archivePositionSetHandler)
        messageRouter.registerHandler(MessageType.CAMERA_BACKGROUND_COLOR, cameraBackgroundColorHandler)
        messageRouter.registerHandler(MessageType.VIRTUAL_KEYBOARD_INPUT, virtualKeyboardInputHandler)
        messageRouter.registerHandler(MessageType.VIRTUAL_JOYSTICK_INPUT, virtualJoystickInputHandler)
        
        Log.i(TAG, "Message handlers registered, starting AIDL client...")
        LogExporter.addLogEntry(TAG, "I", "Starting AIDL client connection attempt")
        
        // Add client handler and start client
        aidlClient.addMessageHandler(aidlClientHandler)
        val startTime = System.currentTimeMillis()
        val result = aidlClient.startClient(context)
        val duration = System.currentTimeMillis() - startTime
        
        if (result) {
            Log.i(TAG, "AIDL client started successfully (took ${duration}ms)")
            LogExporter.addLogEntry(TAG, "I", "AIDL client started successfully in ${duration}ms")
        } else {
            Log.w(TAG, "Failed to start AIDL client (took ${duration}ms)")
            LogExporter.addLogEntry(TAG, "W", "Failed to start AIDL client after ${duration}ms")
            
            // Additional failure diagnosis
            diagnosePotentialConnectionIssues(context)
        }
        
        Log.i(TAG, "=== AIDL Client Setup Diagnosis Complete ===")
    }
    
    private fun diagnosePotentialConnectionIssues(context: Context) {
        Log.w(TAG, "=== Connection Failure Analysis ===")
        LogExporter.addLogEntry(TAG, "W", "Analyzing connection failure")
        
        // Check if service is running
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager
        val runningServices = activityManager.getRunningServices(Integer.MAX_VALUE)
        val serviceRunning = runningServices.any { 
            it.service.className.contains("LinkuraAidlService") 
        }
        Log.w(TAG, "AIDL service currently running: $serviceRunning")
        LogExporter.addLogEntry(TAG, "W", "Service running: $serviceRunning")
        
        // Check SELinux status
        try {
            val selinuxFile = java.io.File("/sys/fs/selinux/enforce")
            if (selinuxFile.exists()) {
                val selinuxStatus = selinuxFile.readText().trim()
                Log.w(TAG, "SELinux enforce status: $selinuxStatus")
                LogExporter.addLogEntry(TAG, "W", "SELinux enforce: $selinuxStatus")
            }
        } catch (e: Exception) {
            Log.w(TAG, "Cannot read SELinux status: ${e.message}")
        }
        
        // Check if we can start the target service manually
        try {
            val serviceIntent = android.content.Intent().apply {
                setClassName("io.github.chocolzs.linkura.localify", "io.github.chocolzs.linkura.localify.ipc.LinkuraAidlService")
            }
            val componentName = context.startService(serviceIntent)
            Log.w(TAG, "Manual service start result: $componentName")
            LogExporter.addLogEntry(TAG, "W", "Manual service start: ${componentName != null}")
        } catch (e: Exception) {
            Log.w(TAG, "Manual service start failed: ${e.message}")
            LogExporter.addLogEntry(TAG, "W", "Manual service start failed: ${e.message}")
        }
    }

    fun initLinkuraConfig(activity: Activity) {
        val intent = activity.intent
        val l4Data = intent.getStringExtra("l4Data")
        val programData = intent.getStringExtra("localData")
        val archiveData = intent.getStringExtra("archiveData")
        val clientResData = intent.getStringExtra("clientResData")
        if (l4Data != null) {
            val readVersion = intent.getStringExtra("lVerName")
            checkPluginVersion(activity, readVersion)

            l4DataInited = true
            val initConfig = try {
                json.decodeFromString<LinkuraConfig>(l4Data)
            }
            catch (e: Exception) {
                null
            }
            // Store the config for later access
            linkuraConfig = initConfig
            val programConfig = try {
                if (programData == null) {
                    ProgramConfig()
                } else {
                    json.decodeFromString<ProgramConfig>(programData)
                }
            }
            catch (e: Exception) {
                null
            }

            // 清理本地文件
            if (programConfig?.cleanLocalAssets == true) {
                FilesChecker.cleanAssets()
            }

            // 检查 files 版本和 assets 版本并更新
            if (programConfig?.checkBuiltInAssets == true) {
                FilesChecker.initAndCheck(activity.filesDir, modulePath)
            }

            // 强制导出 assets 文件
            if (initConfig?.forceExportResource == true) {
                FilesChecker.updateFiles()
            }

            // 使用热更新文件
            if ((programConfig?.useRemoteAssets == true) || (programConfig?.useAPIAssets == true)) {
                // val dataUri = intent.data
                val dataUri = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    intent.getParcelableExtra("resource_file", Uri::class.java)
                } else {
                    @Suppress("DEPRECATION")
                    intent.getParcelableExtra<Uri>("resource_file")
                }

                if (dataUri != null) {
                    if (!externalFilesChecked) {
                        externalFilesChecked = true
                        // Log.d(TAG, "dataUri: $dataUri")
                        FileHotUpdater.updateFilesFromZip(activity, dataUri, activity.filesDir,
                            programConfig.delRemoteAfterUpdate)
                    }
                }
                else if (programConfig.useAPIAssets) {
                    if (!File(activity.filesDir, localizationFilesDir).exists() &&
                        (initConfig?.forceExportResource == false)) {
                        // 使用 API 资源，不检查内置，API 资源无效，且游戏内没有插件数据时，释放内置数据
                        FilesChecker.initAndCheck(activity.filesDir, modulePath)
                    }
                }
            }

            loadConfig(l4Data)
            Log.d(TAG, "l4Data: $l4Data")
            // Load archive configuration if available
        }
        if (archiveData != null) {
            loadArchiveConfig(archiveData)
            Log.d(TAG, "archiveData loaded")
        }
        // Get target app version info
        val packageInfo = activity.packageManager.getPackageInfo(targetPackageName, 0)
        val versionName = packageInfo.versionName
        val versionCode = packageInfo.longVersionCode

        Log.i(TAG, "Game version info - Name: $versionName, Code: $versionCode")
        LogExporter.addLogEntry(TAG, "I", "Game version: $versionName ($versionCode)")
        if (clientResData != null) {
            processClientResourceData(clientResData, versionName)
        }
    }

    private fun processClientResourceData(clientResData: String, currentVersionName: String) {
        val allClientRes = json.decodeFromString<Map<String, List<String>>>(clientResData)
        val clientResList = allClientRes[currentVersionName]
        
        // Find the latest client version (highest version key in the map)
        val latestClientVersion = allClientRes.keys.maxOrNull() ?: currentVersionName
        val latestClientResList = allClientRes[latestClientVersion]
        val latestResVersion = latestClientResList?.lastOrNull() ?: ""
        
        if (clientResList != null && clientResList.isNotEmpty()) {
            Log.i(TAG, "Found ${clientResList.size} client resources for current version $currentVersionName")
            LogExporter.addLogEntry(TAG, "I", "Client resources for $currentVersionName: ${clientResList.size} items")
            
            // Get the last item from the resource list as current res version
            val currentResVersion = clientResList.lastOrNull()
            if (currentResVersion != null) {
                Log.i(TAG, "Current client: $currentVersionName, current res: $currentResVersion")
                Log.i(TAG, "Latest client: $latestClientVersion, latest res: $latestResVersion")
                LogExporter.addLogEntry(TAG, "I", "Current: client=$currentVersionName, res=$currentResVersion")
                LogExporter.addLogEntry(TAG, "I", "Latest: client=$latestClientVersion, res=$latestResVersion")
                
                // Call native function to store all version information
                loadClientResVersion(currentVersionName, currentResVersion, latestClientVersion, latestResVersion)
            } else {
                Log.w(TAG, "Client resources list is empty for version $currentVersionName")
                LogExporter.addLogEntry(TAG, "W", "Empty client resources list for $currentVersionName")
            }
            
            clientResList.forEach { resource ->
                Log.d(TAG, "Client resource: $resource")
            }
        } else {
            Log.w(TAG, "No client resources found for current version $currentVersionName")
            LogExporter.addLogEntry(TAG, "W", "No client resources for version $currentVersionName")
            
            // Even if current version has no resources, still try to load latest version info
            if (latestResVersion.isNotEmpty()) {
                Log.i(TAG, "Loading latest version info only - Latest client: $latestClientVersion, latest res: $latestResVersion")
                LogExporter.addLogEntry(TAG, "I", "Latest version only: client=$latestClientVersion, res=$latestResVersion")
                loadClientResVersion(currentVersionName, "", latestClientVersion, latestResVersion)
            }
        }
    }

    private fun checkPluginVersion(activity: Activity, readVersion: String?) {
        val buildVersionName = BuildConfig.VERSION_NAME
        Log.i(TAG, "Checking Plugin Version: Build: $buildVersionName, Request: $readVersion")
        if (readVersion?.trim() == buildVersionName.trim()) {
            return
        }

        val builder = AlertDialog.Builder(activity)
        val infoBuilder = AlertDialog.Builder(activity)
        builder.setTitle("Warning")
        builder.setCancelable(false)
        builder.setMessage(when (getCurrentLanguage(activity)) {
            "zh" -> "检测到插件版本不一致\n内置版本: $buildVersionName\n请求版本: $readVersion\n\n这可能是使用了 LSPatch 的集成模式，仅更新了插件本体，未重新修补游戏导致的。请使用 $readVersion 版本的插件重新修补或使用本地模式。"
            else -> "Detected plugin version mismatch\nBuilt-in version: $buildVersionName\nRequested version: $readVersion\n\nThis may be caused by using the LSPatch integration mode, where only the plugin itself was updated without re-patching the game. Please re-patch the game using the $readVersion version of the plugin or use the local mode."
        })

        builder.setPositiveButton("OK") { dialog, _ ->
            dialog.dismiss()
        }

        builder.setNegativeButton("Exit") { dialog, _ ->
            dialog.dismiss()
            activity.finishAffinity()
        }

        val dialog = builder.create()

        infoBuilder.setOnCancelListener {
            dialog.show()
        }

        dialog.show()
    }

    private fun showGetConfigFailedImpl(activity: Context, title: String, msg: String, infoButton: String, dlButton: String, okButton: String) {
        if (getConfigError == null) return
        val builder = AlertDialog.Builder(activity)
        val infoBuilder = AlertDialog.Builder(activity)
        val errConfigStr = getConfigError.toString()
        builder.setTitle("$title: $errConfigStr")
        getConfigError = null
        builder.setCancelable(false)
        builder.setMessage(msg)

        builder.setPositiveButton(okButton) { dialog, _ ->
            dialog.dismiss()
        }

        builder.setNegativeButton(dlButton) { dialog, _ ->
            dialog.dismiss()
            val webpage = Uri.parse("https://github.com/ChocoLZS/linkura-localify")
            val intent = Intent(Intent.ACTION_VIEW, webpage)
            activity.startActivity(intent)
        }

        builder.setNeutralButton(infoButton) { _, _ ->
            infoBuilder.setTitle("Error Info")
            infoBuilder.setMessage(errConfigStr)
            val infoDialog = infoBuilder.create()
            infoDialog.show()
        }

        val dialog = builder.create()

        infoBuilder.setOnCancelListener {
            dialog.show()
        }

        dialog.show()
    }

    fun showGetConfigFailed(activity: Context) {
        val langData = when (getCurrentLanguage(activity)) {
            "zh" -> {
                mapOf(
                    "title" to "无法读取设置",
                    "message" to "配置读取失败，将使用默认配置。\n" +
                            "可能是您使用了 LSPatch 等工具的集成模式，也有可能是您拒绝了拉起插件的权限。\n" +
                            "若您使用了 LSPatch 等工具的集成模式，且没有单独安装插件本体，请下载插件本体。\n" +
                            "若您安装了插件本体，却弹出这个错误，请允许本应用拉起其他应用。",
                    "infoButton" to "详情",
                    "dlButton" to "下载",
                    "okButton" to "确定"
                )
            }
            else -> {
                mapOf(
                    "title" to "Get Config Failed",
                    "message" to "Configuration loading failed, the default configuration will be used.\n" +
                            "This might be due to the use the integration mode of LSPatch, or possibly because you denied the permission to launch the plugin.\n" +
                            "If you used the integration mode of LSPatch and did not install the plugin itself separately, please download the plugin.\n" +
                            "If you have installed the plugin but still see this error, please allow this application to launch other applications.",
                    "infoButton" to "Info",
                    "dlButton" to "Download",
                    "okButton" to "OK"
                )
            }
        }
        showGetConfigFailedImpl(activity, langData["title"]!!, langData["message"]!!, langData["infoButton"]!!,
            langData["dlButton"]!!, langData["okButton"]!!)
    }

    private fun getCurrentLanguage(context: Context): String {
        val locale: Locale = context.resources.configuration.locales.get(0)
        return locale.language
    }

    fun requestConfig(activity: Context) {
        try {
            val intent = Intent().apply {
                setClassName("io.github.chocolzs.linkura.localify", "io.github.chocolzs.linkura.localify.TranslucentActivity")
                putExtra("l4Data", "requestConfig")
                flags = FLAG_ACTIVITY_NEW_TASK
            }
            activity.startActivity(intent)
        }
        catch (e: Exception) {
            getConfigError = e
            val fakeActivity = Activity().apply {
                intent = Intent().apply {
                    putExtra("l4Data", "{}")
                }
            }
            initLinkuraConfig(fakeActivity)
        }

    }

    override fun initZygote(startupParam: IXposedHookZygoteInit.StartupParam) {
        modulePath = startupParam.modulePath
    }

    companion object {
        // Camera info loop ignore flag - shared across instances
        @Volatile
        @JvmStatic
        private var sharedIgnoreCameraInfoLoop = false
        
        // Debounce job for pauseCameraInfoLoop
        @JvmStatic
        private var debounceJob: kotlinx.coroutines.Job? = null
        
        @JvmStatic
        external fun initHook(targetLibraryPath: String, localizationFilesDir: String)
        @JvmStatic
        external fun keyboardEvent(keyCode: Int, action: Int)
        @JvmStatic
        external fun joystickEvent(
            action: Int,
            leftStickX: Float,
            leftStickY: Float,
            rightStickX: Float,
            rightStickY: Float,
            leftTrigger: Float,
            rightTrigger: Float,
            hatX: Float,
            hatY: Float
        )
        @JvmStatic
        external fun loadConfig(configJsonStr: String)
        
        @JvmStatic
        external fun loadArchiveConfig(configJsonStr: String)

        @JvmStatic
        external fun loadClientResVersion(currentClientVersion: String, currentResVersion: String,
                                          latestClientVersion: String, latestResVersion: String)

        // Toast快速切换内容
        private var toast: Toast? = null

        @JvmStatic
        fun showToast(message: String) {
            val app = AndroidAppHelper.currentApplication()
            val context = app?.applicationContext
            if (context != null) {
                val handler = Handler(Looper.getMainLooper())
                handler.post {
                    // 取消之前的 Toast
                    toast?.cancel()
                    // 创建新的 Toast
                    toast = Toast.makeText(context, message, Toast.LENGTH_SHORT)
                    // 展示新的 Toast
                    toast?.show()
                }
            }
            else {
                Log.e(TAG, "showToast: $message failed: applicationContext is null")
            }
        }

        @JvmStatic
        external fun pluginCallbackLooper(): Int

        @JvmStatic
        external fun getCameraInfoProtobuf(): ByteArray
        
        @JvmStatic
        external fun updateConfig(configUpdateData: ByteArray)
        
        @JvmStatic
        external fun getCurrentArchiveInfo(): ByteArray
        
        @JvmStatic
        external fun setArchivePosition(seconds: Float)
        
        @JvmStatic
        external fun setCameraBackgroundColor(red: Float, green: Float, blue: Float, alpha: Float)
        
        @OptIn(DelicateCoroutinesApi::class)
        @JvmStatic
        fun pauseCameraInfoLoop(delayMillis: Long = 3000) {
            Log.i(TAG, "pauseCameraInfoLoop called from C++ with delay: ${delayMillis}ms")
            sharedIgnoreCameraInfoLoop = true
            
            // Cancel previous job if exists (debounce)
            debounceJob?.cancel()
            
            // Create new debounce job
            debounceJob = GlobalScope.launch {
                delay(delayMillis) // Wait for specified milliseconds
                sharedIgnoreCameraInfoLoop = false
                Log.i(TAG, "pauseCameraInfoLoop: flag reset to false after ${delayMillis}ms")
            }
        }

        fun getPref(path: String) : XSharedPreferences? {
            val pref = XSharedPreferences(BuildConfig.APPLICATION_ID, path)
            Log.d(TAG, "get perf perf file is ${pref.file.absolutePath}")
            return if(pref.file.canRead()) pref else null
        }

        // lazy loads when needed
        val prefForCameraData by lazy { getPref("camera_data") }
    }

    init {
        ShadowHook.init(
            ConfigBuilder()
                .setMode(ShadowHook.Mode.UNIQUE)
                .build()
        )

        nativeLibLoadSuccess = try {
            System.loadLibrary(nativeLibName)
            true
        } catch (e: UnsatisfiedLinkError) {
            false
        }
    }
}