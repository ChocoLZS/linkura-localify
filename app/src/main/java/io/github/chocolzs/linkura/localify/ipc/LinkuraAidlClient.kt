package io.github.chocolzs.linkura.localify.ipc

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.os.IBinder
import android.os.RemoteException
import de.robv.android.xposed.XposedBridge
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import kotlinx.coroutines.*
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

/**
 * AIDL client specifically for Xposed module usage
 * This replaces the socket-based communication
 */
class LinkuraAidlClient private constructor() {
    companion object {
        private const val TAG = "XposedAidlClient"
        private const val RECONNECT_INTERVAL_MS = 2000L
        private const val SERVICE_PACKAGE = "io.github.chocolzs.linkura.localify"
        private const val SERVICE_CLASS = "io.github.chocolzs.linkura.localify.ipc.LinkuraAidlService"
        
        @Volatile
        private var INSTANCE: LinkuraAidlClient? = null
        
        fun getInstance(): LinkuraAidlClient {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: LinkuraAidlClient().also { INSTANCE = it }
            }
        }
        
        // Xposed logging helper
        private fun log(message: String) {
            XposedBridge.log("$TAG: $message")
        }
        
        private fun logError(message: String, throwable: Throwable? = null) {
            XposedBridge.log("$TAG ERROR: $message")
            throwable?.let { XposedBridge.log(it) }
        }
    }

    private var context: Context? = null
    private var service: ILinkuraService? = null
    private val isConnected = AtomicBoolean(false)
    private val isRunning = AtomicBoolean(false)
    private val messageHandlers = CopyOnWriteArrayList<MessageHandler>()
    private var clientJob: Job? = null
    private var startTime: Long = 0
    private var lastConnectionAttempt: Long = 0
    private var connectionAttempts: Long = 0
    private var successfulConnections: Long = 0
    private var messagesSent: AtomicLong = AtomicLong(0)
    private var messagesReceived: AtomicLong = AtomicLong(0)

    interface MessageHandler {
        fun onMessageReceived(type: MessageType, payload: ByteArray)
        fun onConnected()
        fun onDisconnected()
        fun onConnectionFailed()
    }

    // AIDL callback implementation for Xposed
    private val callback = object : ILinkuraCallback.Stub() {
        override fun onMessageReceived(messageType: Int, payload: ByteArray?) {
            if (payload == null) {
                log("Received null payload for message type $messageType")
                return
            }

            messagesReceived.incrementAndGet()
            val receivedCount = messagesReceived.get()

            log("Message received from server: type=$messageType, size=${payload.size}, total_received=$receivedCount")

            try {
                val messageTypeEnum = MessageType.forNumber(messageType) ?: MessageType.UNKNOWN
                messageHandlers.forEach { handler ->
                    handler.onMessageReceived(messageTypeEnum, payload)
                }

                if (receivedCount % 100 == 0L) {
                    log("Received $receivedCount messages from server")
                }
            } catch (e: Exception) {
                logError("Error handling received message", e)
            }
        }

        override fun onConnected() {
            isConnected.set(true)
            successfulConnections++
            
            log("Successfully connected to AIDL service (attempt #$connectionAttempts)")
            
            try {
                messageHandlers.forEach { it.onConnected() }
                log("Notified ${messageHandlers.size} handlers about successful connection")
            } catch (e: Exception) {
                logError("Error notifying handlers about connection", e)
            }
        }

        override fun onDisconnected() {
            val wasConnected = isConnected.getAndSet(false)
            if (wasConnected) {
                log("Disconnected from AIDL service")
                
                try {
                    messageHandlers.forEach { it.onDisconnected() }
                    log("Notified ${messageHandlers.size} handlers about disconnection")
                } catch (e: Exception) {
                    logError("Error notifying handlers about disconnection", e)
                }
            }
        }

        override fun onConnectionFailed() {
            try {
                messageHandlers.forEach { it.onConnectionFailed() }
                log("Notified ${messageHandlers.size} handlers about connection failure")
            } catch (e: Exception) {
                logError("Error notifying handlers about connection failure", e)
            }
        }
    }

    // Service connection for Xposed
    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            log("Service connected: $name")
            service = ILinkuraService.Stub.asInterface(binder)
            
            try {
                service?.registerCallback(callback)
                // onConnected will be called by the callback
            } catch (e: RemoteException) {
                logError("Failed to register callback", e)
                callback.onConnectionFailed()
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            log("Service disconnected: $name")
            service = null
            callback.onDisconnected()
        }

        override fun onBindingDied(name: ComponentName?) {
            log("Service binding died: $name")
            service = null
            callback.onDisconnected()
        }
    }

    @OptIn(DelicateCoroutinesApi::class)
    fun startClient(context: Context): Boolean {
        if (isRunning.get()) {
            log("Client already running")
            return true
        }

        this.context = context.applicationContext
        
        log("Starting Xposed AIDL client...")
        
        // Reset statistics
        startTime = System.currentTimeMillis()
        lastConnectionAttempt = 0
        connectionAttempts = 0
        successfulConnections = 0
        messagesSent.set(0)
        messagesReceived.set(0)

        isRunning.set(true)
        clientJob = GlobalScope.launch(Dispatchers.Main) {
            runClientLoop()
        }
        
        log("Xposed AIDL client started successfully")
        return true
    }

    private suspend fun runClientLoop() {
        log("Xposed client loop started")
        
        while (isRunning.get() && !Thread.currentThread().isInterrupted) {
            try {
                if (connectToService()) {
                    // Connection successful, wait until disconnected
                    while (isRunning.get() && isConnected.get()) {
                        delay(1000) // Check connection status every second
                    }
                    
                    // Only disconnect if we naturally lost connection (not if we're shutting down)
                    if (isRunning.get() && !isConnected.get()) {
                        log("Connection lost, will attempt to reconnect")
                    }
                } else {
                    // Connection failed
                    callback.onConnectionFailed()
                }
            } catch (e: Exception) {
                log("Client loop error (attempt #$connectionAttempts): ${e.message}")
                disconnect() // Only disconnect on error
            }

            // Only reconnect if we're still running and not connected
            if (isRunning.get() && !isConnected.get()) {
                log("Reconnecting in ${RECONNECT_INTERVAL_MS}ms... (attempt #${connectionAttempts + 1})")
                delay(RECONNECT_INTERVAL_MS)
            }
        }
        
        // Final cleanup when shutting down
        disconnect()
        
        val uptime = System.currentTimeMillis() - startTime
        log("Xposed client loop ended (uptime: ${uptime}ms, attempts: $connectionAttempts, successful: $successfulConnections)")
    }

    private suspend fun connectToService(): Boolean = withContext(Dispatchers.Main) {
        // Don't increment connection attempts or disconnect if already connected
        if (isConnected.get()) {
            return@withContext true
        }
        
        connectionAttempts++
        lastConnectionAttempt = System.currentTimeMillis()
        
        try {
            // Only disconnect if not already disconnected
            if (service != null) {
                disconnect()
            }
            
            log("Connecting to AIDL service... (attempt #$connectionAttempts)")
            
            val intent = Intent().apply {
                component = ComponentName(SERVICE_PACKAGE, SERVICE_CLASS)
            }
            
            val bindResult = context?.bindService(intent, serviceConnection, Context.BIND_AUTO_CREATE)
            
            if (bindResult == true) {
                log("Service binding initiated successfully")
                return@withContext true
            } else {
                logError("Failed to bind to service")
                return@withContext false
            }
            
        } catch (e: Exception) {
            logError("Error connecting to service: ${e.message}", e)
            return@withContext false
        }
    }

    fun sendMessage(type: MessageType, message: com.google.protobuf.MessageLite): Boolean {
        if (!isConnected.get() || service == null) {
            log("Cannot send message: not connected to service (type=$type)")
            return false
        }
        
        return try {
            val envelope = MessageEnvelope.newBuilder()
                .setType(type)
                .setPayload(com.google.protobuf.ByteString.copyFrom(message.toByteArray()))
                .build()
            
            val data = envelope.toByteArray()
            val result = service?.sendMessage(type.number, data) ?: false
            
            if (result) {
                messagesSent.incrementAndGet()
                val sentCount = messagesSent.get()
                
                log("Message sent to service successfully: type=$type, size=${data.size}, total_sent=$sentCount")
                
                if (sentCount % 50 == 0L) {
                    log("Sent $sentCount messages to service")
                }
            } else {
                log("Service rejected message: type=$type")
            }
            
            result
        } catch (e: RemoteException) {
            logError("Error sending message to service (type=$type): ${e.message}", e)
            disconnect()
            false
        } catch (e: Exception) {
            logError("Unexpected error sending message (type=$type): ${e.message}", e)
            false
        }
    }

    fun stopClient() {
        if (!isRunning.getAndSet(false)) {
            log("Client is not running, stop request ignored")
            return
        }
        
        val stopTime = System.currentTimeMillis()
        val uptime = stopTime - startTime
        
        log("Stopping Xposed AIDL client... (uptime: ${uptime}ms)")
        
        try {
            clientJob?.cancel()
            log("Client job cancelled")
        } catch (e: Exception) {
            log("Error cancelling client job: ${e.message}")
        }
        
        disconnect()
        
        log("Xposed AIDL client stopped (final stats: sent=${messagesSent.get()}, received=${messagesReceived.get()}, connections=$successfulConnections/$connectionAttempts)")
    }

    private fun disconnect() {
        if (isConnected.getAndSet(false)) {
            log("Disconnecting from AIDL service...")
            
            try {
                service?.unregisterCallback(callback)
                log("Callback unregistered")
            } catch (e: RemoteException) {
                log("Error unregistering callback: ${e.message}")
            }
            
            try {
                context?.unbindService(serviceConnection)
                log("Service unbound")
            } catch (e: Exception) {
                log("Error unbinding service: ${e.message}")
            }
            
            service = null
            
            log("Disconnected from AIDL service")
        }
    }

    fun addMessageHandler(handler: MessageHandler) {
        messageHandlers.add(handler)
        log("Message handler added: ${handler.javaClass.simpleName}")
    }

    fun removeMessageHandler(handler: MessageHandler) {
        messageHandlers.remove(handler)
        log("Message handler removed: ${handler.javaClass.simpleName}")
    }

    fun isClientConnected(): Boolean = isConnected.get()
    
    fun isClientRunning(): Boolean = isRunning.get()
    
    /**
     * Get detailed client status for debugging
     */
    fun getClientStatus(): String {
        val currentTime = System.currentTimeMillis()
        val uptime = if (startTime > 0) currentTime - startTime else 0
        val connectionSuccessRate = if (connectionAttempts > 0) (successfulConnections * 100.0 / connectionAttempts) else 0.0
        
        return buildString {
            appendLine("=== XposedAidlClient Status ===")
            appendLine("Client Running: ${isRunning.get()}")
            appendLine("Connected to Service: ${isConnected.get()}")
            appendLine("Service Package: $SERVICE_PACKAGE")
            appendLine("Service Class: $SERVICE_CLASS")
            appendLine("Start Time: $startTime")
            appendLine("Uptime: ${uptime}ms")
            appendLine("Connection Attempts: $connectionAttempts")
            appendLine("Successful Connections: $successfulConnections")
            appendLine("Connection Success Rate: ${"%.1f".format(connectionSuccessRate)}%")
            appendLine("Messages Sent: ${messagesSent.get()}")
            appendLine("Messages Received: ${messagesReceived.get()}")
            appendLine("Message Handlers: ${messageHandlers.size}")
            appendLine("Service Interface: ${service != null}")
        }
    }
    
    /**
     * Log current client status for debugging
     */
    fun logClientStatus() {
        val status = getClientStatus()
        log(status)
    }
}