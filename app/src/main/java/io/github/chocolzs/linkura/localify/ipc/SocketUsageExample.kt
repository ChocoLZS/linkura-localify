package io.github.chocolzs.linkura.localify.ipc

import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.models.LinkuraConfig

/**
 * Example usage of the duplex socket communication system
 * This file serves as documentation and example code for developers
 */
object SocketUsageExample {
    private const val TAG = "SocketUsageExample"

    /**
     * Example: Java/Android app side (Server)
     * This would typically be called from MainActivity or a Service
     */
    fun setupJavaAppSide() {
        val socketServer = DuplexSocketServer.getInstance()
        val messageRouter = MessageRouter()
        val configUpdateManager = ConfigUpdateManager.getInstance()

        // Setup message handlers
        val cameraDataHandler = object : MessageRouter.MessageTypeHandler {
            override fun handleMessage(payload: ByteArray): Boolean {
                return try {
                    val cameraData = CameraData.parseFrom(payload)
                    Log.d(TAG, "Received camera data: position=(${cameraData.position.x}, ${cameraData.position.y}, ${cameraData.position.z})")
                    // Update UI with camera data
                    true
                } catch (e: Exception) {
                    Log.e(TAG, "Error handling camera data", e)
                    false
                }
            }
        }

        val serverHandler = object : DuplexSocketServer.MessageHandler {
            override fun onMessageReceived(type: MessageType, payload: ByteArray) {
                messageRouter.routeMessage(type, payload)
            }

            override fun onClientConnected() {
                Log.i(TAG, "XPosed client connected")
                // Send overlay start command
                val overlayControl = OverlayControl.newBuilder()
                    .setAction(OverlayAction.START_OVERLAY)
                    .build()
                socketServer.sendMessage(MessageType.OVERLAY_CONTROL, overlayControl)
            }

            override fun onClientDisconnected() {
                Log.i(TAG, "XPosed client disconnected")
            }
        }

        // Register handlers
        messageRouter.registerHandler(MessageType.CAMERA_DATA, cameraDataHandler)
        socketServer.addMessageHandler(serverHandler)

        // Start server
        if (socketServer.startServer()) {
            Log.i(TAG, "Socket server started successfully")
        }

        // Example: Send config update
        val config = LinkuraConfig(
            enabled = true,
            dbgMode = false,
            renderHighResolution = true
        )
        configUpdateManager.sendConfigUpdate(config)
    }

    /**
     * Example: XPosed/Game hook side (Client)
     * This would typically be called from LinkuraHookMain
     */
    fun setupXPosedSide() {
        val socketClient = DuplexSocketClient.getInstance()
        val messageRouter = MessageRouter()
        var isCameraDataLoopEnabled = false

        // Setup message handlers
        val configUpdateHandler = object : MessageRouter.MessageTypeHandler {
            override fun handleMessage(payload: ByteArray): Boolean {
                return try {
                    val configUpdate = ConfigUpdate.parseFrom(payload)
                    Log.d(TAG, "Received config update: enabled=${configUpdate.enabled}")
                    // Apply config to native layer
                    // updateConfig(payload) // Call native function
                    true
                } catch (e: Exception) {
                    Log.e(TAG, "Error handling config update", e)
                    false
                }
            }
        }

        val overlayControlHandler = object : MessageRouter.MessageTypeHandler {
            override fun handleMessage(payload: ByteArray): Boolean {
                return try {
                    val overlayControl = OverlayControl.parseFrom(payload)
                    when (overlayControl.action) {
                        OverlayAction.START_OVERLAY -> {
                            isCameraDataLoopEnabled = true
                            Log.i(TAG, "Camera data loop enabled")
                        }
                        OverlayAction.STOP_OVERLAY -> {
                            isCameraDataLoopEnabled = false
                            Log.i(TAG, "Camera data loop disabled")
                        }
                        else -> return false
                    }
                    true
                } catch (e: Exception) {
                    Log.e(TAG, "Error handling overlay control", e)
                    false
                }
            }
        }

        val clientHandler = object : DuplexSocketClient.MessageHandler {
            override fun onMessageReceived(type: MessageType, payload: ByteArray) {
                messageRouter.routeMessage(type, payload)
            }

            override fun onConnected() {
                Log.i(TAG, "Connected to Java app server")
            }

            override fun onDisconnected() {
                Log.i(TAG, "Disconnected from Java app server")
                isCameraDataLoopEnabled = false
            }

            override fun onConnectionFailed() {
                Log.w(TAG, "Failed to connect to Java app server")
            }
        }

        // Register handlers
        messageRouter.registerHandler(MessageType.CONFIG_UPDATE, configUpdateHandler)
        messageRouter.registerHandler(MessageType.OVERLAY_CONTROL, overlayControlHandler)
        socketClient.addMessageHandler(clientHandler)

        // Start client
        if (socketClient.startClient()) {
            Log.i(TAG, "Socket client started successfully")
        }

        // Example: Send camera data in game loop
        // This would typically be called from the main game loop
        fun sendCameraDataIfEnabled() {
            if (isCameraDataLoopEnabled && socketClient.isClientConnected()) {
                // val protobufData = getCameraInfoProtobuf() // Get from native
                // val cameraData = CameraData.parseFrom(protobufData)
                // socketClient.sendMessage(MessageType.CAMERA_DATA, cameraData)
            }
        }
    }

    /**
     * Cleanup example - call when shutting down
     */
    fun cleanup() {
        // Server side cleanup
        val socketServer = DuplexSocketServer.getInstance()
        socketServer.stopServer()

        // Client side cleanup
        val socketClient = DuplexSocketClient.getInstance()
        socketClient.stopClient()
    }
}