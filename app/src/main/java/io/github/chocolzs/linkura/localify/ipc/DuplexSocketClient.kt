package io.github.chocolzs.linkura.localify.ipc

import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.mainUtils.LogExporter
import kotlinx.coroutines.*
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.IOException
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicBoolean

@Deprecated(
    message = "This class is deprecated and will be removed in future versions.",
    replaceWith = ReplaceWith("LinkuraAidlClient()"),
    level = DeprecationLevel.WARNING
)
class DuplexSocketClient private constructor() {
    companion object {
        private const val TAG = "DuplexSockesendMessagetClient"
        private const val SOCKET_ADDRESS = "linkura_duplex_socket"
        private const val RECONNECT_INTERVAL_MS = 2000L
        
        @Volatile
        private var INSTANCE: DuplexSocketClient? = null
        
        fun getInstance(): DuplexSocketClient {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: DuplexSocketClient().also { INSTANCE = it }
            }
        }
    }

    private var clientSocket: LocalSocket? = null
    private var outputStream: BufferedOutputStream? = null
    private val isConnected = AtomicBoolean(false)
    private val isRunning = AtomicBoolean(false)
    private val messageHandlers = CopyOnWriteArrayList<MessageHandler>()
    private var clientJob: Job? = null
    private var startTime: Long = 0
    private var lastConnectionAttempt: Long = 0
    private var connectionAttempts: Long = 0
    private var successfulConnections: Long = 0
    private var messagesSent: Long = 0
    private var messagesReceived: Long = 0
    private var totalDowntime: Long = 0
    private var lastDisconnectTime: Long = 0

    interface MessageHandler {
        fun onMessageReceived(type: MessageType, payload: ByteArray)
        fun onConnected()
        fun onDisconnected()
        fun onConnectionFailed()
    }

    @OptIn(DelicateCoroutinesApi::class)
    fun startClient(): Boolean {
        if (isRunning.get()) {
            Log.w(TAG, "Client already running")
            LogExporter.addLogEntry(TAG, "W", "Attempted to start client but it's already running")
            return true
        }

        Log.i(TAG, "Starting duplex socket client...")
        LogExporter.addLogEntry(TAG, "I", "Starting duplex socket client to $SOCKET_ADDRESS")
        
        // Reset statistics
        startTime = System.currentTimeMillis()
        lastConnectionAttempt = 0
        connectionAttempts = 0
        successfulConnections = 0
        messagesSent = 0
        messagesReceived = 0
        totalDowntime = 0
        lastDisconnectTime = 0

        isRunning.set(true)
        clientJob = GlobalScope.launch(Dispatchers.IO) {
            runClientLoop()
        }
        
        Log.i(TAG, "Duplex socket client started successfully")
        LogExporter.addLogEntry(TAG, "I", "Duplex socket client started successfully")
        return true
    }

    private suspend fun runClientLoop() {
        Log.i(TAG, "Client loop started")
        LogExporter.addLogEntry(TAG, "I", "Client loop started")
        
        while (isRunning.get() && !Thread.currentThread().isInterrupted) {
            try {
                if (connectToServer()) {
                    handleServerCommunication()
                } else {
                    // Notify handlers about connection failure
                    try {
                        messageHandlers.forEach { it.onConnectionFailed() }
                        Log.d(TAG, "Notified ${messageHandlers.size} handlers about connection failure")
                    } catch (e: Exception) {
                        Log.e(TAG, "Error notifying handlers about connection failure", e)
                        LogExporter.addLogEntry(TAG, "E", "Error notifying handlers about connection failure: ${e.message}")
                    }
                }
            } catch (e: Exception) {
                Log.w(TAG, "Client loop error (attempt #$connectionAttempts): ${e.message}", e)
                LogExporter.addLogEntry(TAG, "W", "Client loop error: ${e.message}")
            } finally {
                disconnect()
            }

            if (isRunning.get()) {
                Log.d(TAG, "Reconnecting in ${RECONNECT_INTERVAL_MS}ms... (attempt #${connectionAttempts + 1})")
                LogExporter.addLogEntry(TAG, "D", "Waiting ${RECONNECT_INTERVAL_MS}ms before reconnection attempt #${connectionAttempts + 1}")
                
                val delayStart = System.currentTimeMillis()
                delay(RECONNECT_INTERVAL_MS)
                val actualDelay = System.currentTimeMillis() - delayStart
                
                if (actualDelay > RECONNECT_INTERVAL_MS + 100) {
                    Log.w(TAG, "Reconnect delay took longer than expected: ${actualDelay}ms vs ${RECONNECT_INTERVAL_MS}ms")
                    LogExporter.addLogEntry(TAG, "W", "Reconnect delay was longer than expected: ${actualDelay}ms")
                }
            }
        }
        
        val uptime = System.currentTimeMillis() - startTime
        Log.i(TAG, "Client loop ended (uptime: ${uptime}ms, attempts: $connectionAttempts, successful: $successfulConnections)")
        LogExporter.addLogEntry(TAG, "I", "Client loop ended (uptime: ${uptime}ms, attempts: $connectionAttempts, successful: $successfulConnections)")
    }

    private fun connectToServer(): Boolean {
        connectionAttempts++
        lastConnectionAttempt = System.currentTimeMillis()
        
        // Calculate downtime if this isn't the first connection
        if (lastDisconnectTime > 0) {
            totalDowntime += (lastConnectionAttempt - lastDisconnectTime)
        }
        
        try {
            disconnect()
            
            Log.d(TAG, "Connecting to server at $SOCKET_ADDRESS... (attempt #$connectionAttempts)")
            LogExporter.addLogEntry(TAG, "D", "Connection attempt #$connectionAttempts to $SOCKET_ADDRESS")
            
            val connectStartTime = System.currentTimeMillis()
            
            clientSocket = LocalSocket().apply {
                connect(LocalSocketAddress(SOCKET_ADDRESS))
            }
            
            val connectionTime = System.currentTimeMillis() - connectStartTime
            
            try {
                outputStream = BufferedOutputStream(clientSocket?.outputStream)
                Log.d(TAG, "Output stream initialized successfully")
            } catch (e: IOException) {
                Log.e(TAG, "Failed to initialize output stream", e)
                LogExporter.addLogEntry(TAG, "E", "Failed to initialize output stream: ${e.message}")
                disconnect()
                return false
            }
            
            isConnected.set(true)
            successfulConnections++
            
            Log.i(TAG, "Successfully connected to duplex server (time: ${connectionTime}ms, attempt #$connectionAttempts)")
            LogExporter.addLogEntry(TAG, "I", "Connected to duplex server (time: ${connectionTime}ms, attempt #$connectionAttempts, total successful: $successfulConnections)")
            
            // Notify handlers about successful connection
            try {
                messageHandlers.forEach { it.onConnected() }
                Log.d(TAG, "Notified ${messageHandlers.size} handlers about successful connection")
            } catch (e: Exception) {
                Log.e(TAG, "Error notifying handlers about connection", e)
                LogExporter.addLogEntry(TAG, "E", "Error notifying handlers about connection: ${e.message}")
            }
            
            return true
            
        } catch (e: IOException) {
            val connectionTime = System.currentTimeMillis() - lastConnectionAttempt
            Log.w(TAG, "Failed to connect to server (attempt #$connectionAttempts, time: ${connectionTime}ms): ${e.message}")
            LogExporter.addLogEntry(TAG, "W", "Connection failed (attempt #$connectionAttempts): ${e.message}")
            disconnect()
            return false
        } catch (e: Exception) {
            val connectionTime = System.currentTimeMillis() - lastConnectionAttempt
            Log.e(TAG, "Unexpected error connecting to server (attempt #$connectionAttempts, time: ${connectionTime}ms): ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Unexpected connection error: ${e.message}")
            disconnect()
            return false
        }
    }

    private suspend fun handleServerCommunication() {
        Log.d(TAG, "Starting server communication handling")
        LogExporter.addLogEntry(TAG, "D", "Starting server communication handling")
        
        val inputStream = BufferedInputStream(clientSocket?.inputStream)
        var messagesProcessed = 0L
        val communicationStartTime = System.currentTimeMillis()
        
        try {
            while (isRunning.get() && isConnected.get() && !Thread.currentThread().isInterrupted) {
                // Read message size (4 bytes)
                val sizeBytes = ByteArray(4)
                val bytesRead = inputStream.read(sizeBytes)
                
                if (bytesRead == -1) {
                    Log.d(TAG, "Server disconnected (EOF reached)")
                    LogExporter.addLogEntry(TAG, "D", "Server disconnected (EOF reached)")
                    break
                }
                
                if (bytesRead != 4) {
                    Log.e(TAG, "Invalid size header: read $bytesRead bytes, expected 4")
                    LogExporter.addLogEntry(TAG, "E", "Invalid size header from server")
                    break
                }
                
                val messageSize = ((sizeBytes[0].toInt() and 0xFF) shl 24) or
                                ((sizeBytes[1].toInt() and 0xFF) shl 16) or
                                ((sizeBytes[2].toInt() and 0xFF) shl 8) or
                                (sizeBytes[3].toInt() and 0xFF)
                
                if (messageSize <= 0 || messageSize > 1024 * 1024) { // Max 1MB
                    Log.e(TAG, "Invalid message size: $messageSize")
                    LogExporter.addLogEntry(TAG, "E", "Invalid message size from server: $messageSize")
                    break
                }
                
                Log.v(TAG, "Reading message of size: $messageSize bytes")
                
                // Read message data
                val messageData = ByteArray(messageSize)
                var totalRead = 0
                val readStartTime = System.currentTimeMillis()
                
                while (totalRead < messageSize) {
                    val read = inputStream.read(messageData, totalRead, messageSize - totalRead)
                    if (read == -1) {
                        Log.e(TAG, "Unexpected EOF while reading message data")
                        LogExporter.addLogEntry(TAG, "E", "Unexpected EOF from server while reading message")
                        break
                    }
                    totalRead += read
                }
                
                val readTime = System.currentTimeMillis() - readStartTime
                
                if (totalRead != messageSize) {
                    Log.e(TAG, "Incomplete message read: $totalRead/$messageSize (read time: ${readTime}ms)")
                    LogExporter.addLogEntry(TAG, "E", "Incomplete message read from server")
                    break
                }
                
                // Parse and handle message
                try {
                    val parseStartTime = System.currentTimeMillis()
                    val envelope = MessageEnvelope.parseFrom(messageData)
                    val parseTime = System.currentTimeMillis() - parseStartTime
                    
                    messagesReceived++
                    messagesProcessed++
                    
                    Log.v(TAG, "Message received from server: type=${envelope.type}, size=${messageSize}, parse_time=${parseTime}ms")
                    
                    val handlingStartTime = System.currentTimeMillis()
                    messageHandlers.forEach { 
                        it.onMessageReceived(envelope.type, envelope.payload.toByteArray())
                    }
                    val handlingTime = System.currentTimeMillis() - handlingStartTime
                    
                    Log.v(TAG, "Message handled: type=${envelope.type}, handlers=${messageHandlers.size}, handling_time=${handlingTime}ms")
                    
                    // Log statistics every 100 messages
                    if (messagesProcessed % 100 == 0L) {
                        val avgTime = (System.currentTimeMillis() - communicationStartTime) / messagesProcessed
                        LogExporter.addLogEntry(TAG, "D", "Processed $messagesProcessed messages from server (avg: ${avgTime}ms/msg)")
                    }
                    
                } catch (e: Exception) {
                    Log.e(TAG, "Error parsing/handling message from server: ${e.message}", e)
                    LogExporter.addLogEntry(TAG, "E", "Error parsing/handling server message: ${e.message}")
                    // Continue processing other messages
                }
            }
            
            val totalTime = System.currentTimeMillis() - communicationStartTime
            Log.i(TAG, "Server communication ended. Stats: $messagesProcessed messages in ${totalTime}ms")
            LogExporter.addLogEntry(TAG, "I", "Server communication ended. Processed $messagesProcessed messages in ${totalTime}ms")
            
        } catch (e: IOException) {
            if (isRunning.get() && isConnected.get()) {
                Log.w(TAG, "Server communication error: ${e.message}", e)
                LogExporter.addLogEntry(TAG, "W", "Server communication error: ${e.message}")
            } else {
                Log.d(TAG, "Server communication ended normally")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error in server communication: ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Unexpected error in server communication: ${e.message}")
        }
    }

    fun sendMessage(type: MessageType, message: com.google.protobuf.MessageLite): Boolean {
        if (!isConnected.get() || outputStream == null) {
            Log.w(TAG, "Cannot send message: not connected to server (type=$type)")
            LogExporter.addLogEntry(TAG, "W", "Cannot send message: not connected (type=$type)")
            return false
        }
        
        val sendStartTime = System.currentTimeMillis()
        
        return try {
            val envelope = MessageEnvelope.newBuilder()
                .setType(type)
                .setPayload(com.google.protobuf.ByteString.copyFrom(message.toByteArray()))
                .build()
            
            val data = envelope.toByteArray()
            val sizeBytes = ByteArray(4)
            sizeBytes[0] = (data.size shr 24).toByte()
            sizeBytes[1] = (data.size shr 16).toByte()
            sizeBytes[2] = (data.size shr 8).toByte()
            sizeBytes[3] = data.size.toByte()
            
            synchronized(this) {
                outputStream?.apply {
                    write(sizeBytes)
                    write(data)
                    flush()
                }
            }
            
            messagesSent++
            val sendTime = System.currentTimeMillis() - sendStartTime
            
            Log.v(TAG, "Message sent to server successfully: type=$type, size=${data.size}, time=${sendTime}ms")
            
            // Log detailed stats every 50 sent messages
            if (messagesSent % 50 == 0L) {
                LogExporter.addLogEntry(TAG, "D", "Sent $messagesSent messages to server (last: type=$type, size=${data.size})")
            }
            
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error sending message to server (type=$type): ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Error sending message to server (type=$type): ${e.message}")
            disconnect()
            false
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error sending message (type=$type): ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Unexpected error sending message (type=$type): ${e.message}")
            false
        }
    }

    fun stopClient() {
        if (!isRunning.getAndSet(false)) {
            Log.w(TAG, "Client is not running, stop request ignored")
            return
        }
        
        val stopTime = System.currentTimeMillis()
        val uptime = stopTime - startTime
        
        Log.i(TAG, "Stopping duplex socket client... (uptime: ${uptime}ms)")
        LogExporter.addLogEntry(TAG, "I", "Stopping client (uptime: ${uptime}ms, attempts: $connectionAttempts, successful: $successfulConnections)")
        
        try {
            clientJob?.cancel()
            Log.d(TAG, "Client job cancelled")
        } catch (e: Exception) {
            Log.w(TAG, "Error cancelling client job: ${e.message}")
        }
        
        disconnect()
        
        Log.i(TAG, "Duplex socket client stopped (final stats: sent=$messagesSent, received=$messagesReceived, connections=$successfulConnections/$connectionAttempts)")
        LogExporter.addLogEntry(TAG, "I", "Duplex socket client stopped successfully")
    }

    private fun disconnect() {
        if (isConnected.getAndSet(false)) {
            lastDisconnectTime = System.currentTimeMillis()
            val sessionDuration = if (lastConnectionAttempt > 0) lastDisconnectTime - lastConnectionAttempt else 0
            
            Log.i(TAG, "Disconnecting from server... (session duration: ${sessionDuration}ms)")
            LogExporter.addLogEntry(TAG, "I", "Disconnecting from server (session: ${sessionDuration}ms, sent: $messagesSent, received: $messagesReceived)")
            
            try {
                outputStream?.close()
                Log.d(TAG, "Output stream closed")
            } catch (e: IOException) {
                Log.w(TAG, "Error closing output stream: ${e.message}")
            }
            
            try {
                clientSocket?.close()
                Log.d(TAG, "Client socket closed")
            } catch (e: IOException) {
                Log.w(TAG, "Error closing client socket: ${e.message}")
            }
            
            outputStream = null
            clientSocket = null
            
            // Notify handlers about disconnection
            try {
                messageHandlers.forEach { it.onDisconnected() }
                Log.d(TAG, "Notified ${messageHandlers.size} handlers about disconnection")
            } catch (e: Exception) {
                Log.e(TAG, "Error notifying handlers about disconnection", e)
                LogExporter.addLogEntry(TAG, "E", "Error notifying handlers about disconnection: ${e.message}")
            }
            
            Log.i(TAG, "Disconnected from duplex server")
        }
    }

    fun addMessageHandler(handler: MessageHandler) {
        messageHandlers.add(handler)
        Log.d(TAG, "Message handler added: ${handler.javaClass.simpleName}")
    }

    fun removeMessageHandler(handler: MessageHandler) {
        messageHandlers.remove(handler)
        Log.d(TAG, "Message handler removed: ${handler.javaClass.simpleName}")
    }

    fun isClientConnected(): Boolean = isConnected.get()
    
    fun isClientRunning(): Boolean = isRunning.get()
    
    /**
     * Get detailed client status for debugging
     */
    fun getClientStatus(): String {
        val currentTime = System.currentTimeMillis()
        val uptime = if (startTime > 0) currentTime - startTime else 0
        val timeSinceLastConnection = if (lastConnectionAttempt > 0) currentTime - lastConnectionAttempt else -1
        val timeSinceLastDisconnect = if (lastDisconnectTime > 0) currentTime - lastDisconnectTime else -1
        val connectionSuccessRate = if (connectionAttempts > 0) (successfulConnections * 100.0 / connectionAttempts) else 0.0
        
        return buildString {
            appendLine("=== DuplexSocketClient Status ===")
            appendLine("Client Running: ${isRunning.get()}")
            appendLine("Connected to Server: ${isConnected.get()}")
            appendLine("Socket Address: $SOCKET_ADDRESS")
            appendLine("Reconnect Interval: ${RECONNECT_INTERVAL_MS}ms")
            appendLine("Start Time: $startTime")
            appendLine("Uptime: ${uptime}ms")
            appendLine("Connection Attempts: $connectionAttempts")
            appendLine("Successful Connections: $successfulConnections")
            appendLine("Connection Success Rate: ${"%.1f".format(connectionSuccessRate)}%")
            appendLine("Messages Sent: $messagesSent")
            appendLine("Messages Received: $messagesReceived")
            appendLine("Last Connection Attempt: $lastConnectionAttempt")
            appendLine("Time Since Last Connection: ${timeSinceLastConnection}ms")
            appendLine("Last Disconnect Time: $lastDisconnectTime")
            appendLine("Time Since Last Disconnect: ${timeSinceLastDisconnect}ms")
            appendLine("Total Downtime: ${totalDowntime}ms")
            appendLine("Message Handlers: ${messageHandlers.size}")
            appendLine("Client Socket: ${clientSocket != null}")
            appendLine("Output Stream: ${outputStream != null}")
            appendLine("Client Job Active: ${clientJob?.isActive ?: false}")
        }
    }
    
    /**
     * Log current client status for debugging
     */
    fun logClientStatus() {
        val status = getClientStatus()
        Log.d(TAG, status)
        LogExporter.addLogEntry(TAG, "D", "Client status requested")
        
        // Also add key metrics to LogExporter
        val currentTime = System.currentTimeMillis()
        val uptime = if (startTime > 0) currentTime - startTime else 0
        val connectionSuccessRate = if (connectionAttempts > 0) (successfulConnections * 100.0 / connectionAttempts) else 0.0
        LogExporter.addLogEntry(TAG, "STATUS", "Client running=${isRunning.get()}, connected=${isConnected.get()}, uptime=${uptime}ms, attempts=$connectionAttempts, successful=$successfulConnections (${connectionSuccessRate.toInt()}%), sent=$messagesSent, received=$messagesReceived")
    }
}