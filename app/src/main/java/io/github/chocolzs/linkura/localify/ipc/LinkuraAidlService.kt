package io.github.chocolzs.linkura.localify.ipc

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.os.RemoteCallbackList
import android.os.RemoteException
import android.util.Log
import io.github.chocolzs.linkura.localify.mainUtils.LogExporter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong

class LinkuraAidlService : Service() {
    companion object {
        private const val TAG = "LinkuraAidlService"
        
        @Volatile
        private var INSTANCE: LinkuraAidlService? = null
        
        fun getInstance(): LinkuraAidlService? = INSTANCE
    }

    private val isRunning = AtomicBoolean(false)
    private val startTime = AtomicLong(0)
    private val messagesSent = AtomicLong(0)
    private val messagesReceived = AtomicLong(0)
    private val callbacks = RemoteCallbackList<ILinkuraCallback>()
    private val messageRouter: MessageRouter by lazy { MessageRouter() }

    // AIDL implementation
    val binder = object : ILinkuraService.Stub() {
        
        override fun registerCallback(callback: ILinkuraCallback?) {
            callback?.let {
                callbacks.register(it)
                val clientCount = callbacks.registeredCallbackCount
                Log.i(TAG, "Client registered. Total clients: $clientCount")
                LogExporter.addLogEntry(TAG, "I", "Client registered. Total clients: $clientCount")
                
                // Notify the new client that it's connected
                try {
                    it.onConnected()
                } catch (e: RemoteException) {
                    Log.w(TAG, "Failed to notify new client of connection", e)
                    callbacks.unregister(it)
                }
            }
        }

        override fun unregisterCallback(callback: ILinkuraCallback?) {
            callback?.let {
                callbacks.unregister(it)
                val clientCount = callbacks.registeredCallbackCount
                Log.i(TAG, "Client unregistered. Total clients: $clientCount")
                LogExporter.addLogEntry(TAG, "I", "Client unregistered. Total clients: $clientCount")
                
                // Notify the client that it's disconnected
                try {
                    it.onDisconnected()
                } catch (e: RemoteException) {
                    Log.v(TAG, "Client already disconnected")
                }
            }
        }

        override fun sendMessage(messageType: Int, payload: ByteArray?): Boolean {
            if (!isRunning.get()) {
                Log.w(TAG, "Service not running, message rejected")
                return false
            }

            if (payload == null) {
                Log.w(TAG, "Null payload received for message type $messageType")
                return false
            }

            messagesReceived.incrementAndGet()
            val receivedCount = messagesReceived.get()

            Log.v(TAG, "Message received: type=$messageType, size=${payload.size}, total_received=$receivedCount")

            // Broadcast to all registered callbacks
            return broadcastMessage(messageType, payload)
        }

        override fun receiveMessage(messageType: Int, payload: ByteArray?): Boolean {
            // First try to route through MessageRouter
            try {
                val messageTypeEnum = MessageType.forNumber(messageType)
                if (messageTypeEnum != null && payload?.isNotEmpty() == true
                        && messageRouter.routeMessage(messageTypeEnum, payload)) {
                    Log.d(TAG, "Message handled by MessageRouter: type=$messageType")
                    return true
                }
            } catch (e: Exception) {
                Log.w(TAG, "Error routing message through MessageRouter", e)
            }
            return false
        }

        override fun getServerStatus(): String {
            val currentTime = System.currentTimeMillis()
            val uptime = if (startTime.get() > 0) currentTime - startTime.get() else 0
            val clientCount = callbacks.registeredCallbackCount

            return buildString {
                appendLine("=== LinkuraAidlService Status ===")
                appendLine("Service Running: ${isRunning.get()}")
                appendLine("Connected Clients: $clientCount")
                appendLine("Start Time: ${startTime.get()}")
                appendLine("Uptime: ${uptime}ms")
                appendLine("Messages Sent: ${messagesSent.get()}")
                appendLine("Messages Received: ${messagesReceived.get()}")
                appendLine("Service PID: ${android.os.Process.myPid()}")
            }
        }

        override fun isReady(): Boolean = isRunning.get()

        override fun getClientCount(): Int = callbacks.registeredCallbackCount
    }

    override fun onCreate() {
        super.onCreate()
        INSTANCE = this
        isRunning.set(true)
        startTime.set(System.currentTimeMillis())
        
        // Set service instance for ConfigUpdateManager
        ConfigUpdateManager.getInstance().setServiceInstance(this.binder)
        
        Log.i(TAG, "LinkuraAidlService created")
        LogExporter.addLogEntry(TAG, "I", "AIDL service created successfully")
    }

    override fun onBind(intent: Intent?): IBinder {
        Log.i(TAG, "Client binding to LinkuraAidlService")
        LogExporter.addLogEntry(TAG, "I", "Client binding to AIDL service")
        return binder
    }

    override fun onUnbind(intent: Intent?): Boolean {
        Log.i(TAG, "Client unbinding from LinkuraAidlService")
        LogExporter.addLogEntry(TAG, "I", "Client unbinding from AIDL service")
        return super.onUnbind(intent)
    }

    override fun onDestroy() {
        super.onDestroy()
        INSTANCE = null
        isRunning.set(false)
        
        // Notify all clients of disconnection
        val clientCount = callbacks.registeredCallbackCount
        val callbacksCopy = arrayOfNulls<ILinkuraCallback>(clientCount)
        for (i in 0 until clientCount) {
            callbacksCopy[i] = callbacks.getBroadcastItem(i)
        }
        
        callbacksCopy.forEach { callback ->
            try {
                callback?.onDisconnected()
            } catch (e: RemoteException) {
                Log.v(TAG, "Client already disconnected during service destruction")
            }
        }
        
        callbacks.kill()
        
        val uptime = System.currentTimeMillis() - startTime.get()
        Log.i(TAG, "LinkuraAidlService destroyed (uptime: ${uptime}ms, sent: ${messagesSent.get()}, received: ${messagesReceived.get()})")
        LogExporter.addLogEntry(TAG, "I", "AIDL service destroyed (uptime: ${uptime}ms)")
    }

    /**
     * Broadcast a message to all registered clients
     */
    fun broadcastMessage(messageType: Int, payload: ByteArray): Boolean {
        val clientCount = callbacks.beginBroadcast()
        var successCount = 0
        
        try {
            for (i in 0 until clientCount) {
                try {
                    callbacks.getBroadcastItem(i)?.onMessageReceived(messageType, payload)
                    successCount++
                } catch (e: RemoteException) {
                    Log.w(TAG, "Failed to send message to client $i", e)
                    // Client is dead, will be cleaned up automatically
                }
            }
        } finally {
            callbacks.finishBroadcast()
        }

        messagesSent.incrementAndGet()
        val sentCount = messagesSent.get()

        Log.v(TAG, "Message broadcasted: type=$messageType, clients=$clientCount, successful=$successCount, total_sent=$sentCount")

        if (sentCount % 50 == 0L) {
            LogExporter.addLogEntry(TAG, "D", "Broadcasted $sentCount messages to clients (last: type=$messageType, size=${payload.size})")
        }

        return successCount > 0
    }

    /**
     * Register a message handler for a specific message type
     */
    fun registerMessageHandler(messageType: MessageType, handler: MessageRouter.MessageTypeHandler) {
        messageRouter.registerHandler(messageType, handler)
        Log.d(TAG, "Message handler registered for type: $messageType")
    }
    
    /**
     * Unregister a message handler for a specific message type
     */
    fun unregisterMessageHandler(messageType: MessageType, handler: MessageRouter.MessageTypeHandler) {
        messageRouter.unregisterHandler(messageType, handler)
        Log.d(TAG, "Message handler unregistered for type: $messageType")
    }
}