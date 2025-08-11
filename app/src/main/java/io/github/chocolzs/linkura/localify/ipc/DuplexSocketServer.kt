package io.github.chocolzs.linkura.localify.ipc

import android.net.LocalServerSocket
import android.net.LocalSocket
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
    replaceWith = ReplaceWith("LinkuraAidlServer()"),
    level = DeprecationLevel.WARNING
)
class DuplexSocketServer private constructor() {
    companion object {
        private const val TAG = "DuplexSocketServer"
        private const val SOCKET_ADDRESS = "linkura_duplex_socket"
        
        @Volatile
        private var INSTANCE: DuplexSocketServer? = null
        
        fun getInstance(): DuplexSocketServer {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: DuplexSocketServer().also { INSTANCE = it }
            }
        }
    }

    private var serverSocket: LocalServerSocket? = null
    private var clientSocket: LocalSocket? = null
    private var outputStream: BufferedOutputStream? = null
    private val isRunning = AtomicBoolean(false)
    private val isClientConnected = AtomicBoolean(false)
    private val messageHandlers = CopyOnWriteArrayList<MessageHandler>()
    private var serverJob: Job? = null
    private var startTime: Long = 0
    private var lastHeartbeatTime: Long = 0
    private var messagesSent: Long = 0
    private var messagesReceived: Long = 0
    private var connectionAttempts: Long = 0

    interface MessageHandler {
        fun onMessageReceived(type: MessageType, payload: ByteArray)
        fun onClientConnected()
        fun onClientDisconnected()
    }

    @OptIn(DelicateCoroutinesApi::class)
    fun startServer(): Boolean {
        if (isRunning.get()) {
            Log.w(TAG, "Server already running")
            LogExporter.addLogEntry(TAG, "W", "Attempted to start server but it's already running")
            return true
        }

        Log.i(TAG, "Attempting to start duplex socket server...")
        LogExporter.addLogEntry(TAG, "I", "Attempting to start duplex socket server at $SOCKET_ADDRESS")

        return try {
            // Check if socket address is already in use
            try {
                val testSocket = LocalServerSocket(SOCKET_ADDRESS)
                testSocket.close()
                Log.d(TAG, "Socket address is available")
            } catch (e: IOException) {
                Log.w(TAG, "Socket address may be in use, attempting cleanup: ${e.message}")
                LogExporter.addLogEntry(TAG, "W", "Socket address may be in use: ${e.message}")
            }
            
            serverSocket = LocalServerSocket(SOCKET_ADDRESS)
            isRunning.set(true)
            startTime = System.currentTimeMillis()
            lastHeartbeatTime = startTime
            messagesSent = 0
            messagesReceived = 0
            connectionAttempts = 0
            
            serverJob = GlobalScope.launch(Dispatchers.IO) {
                runServerLoop()
            }
            
            Log.i(TAG, "Duplex socket server started successfully at $SOCKET_ADDRESS")
            LogExporter.addLogEntry(TAG, "I", "Duplex socket server started successfully at $SOCKET_ADDRESS")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Failed to start server: ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Failed to start server: ${e.message}")
            cleanup()
            false
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error starting server: ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Unexpected error starting server: ${e.message}")
            cleanup()
            false
        }
    }

    private suspend fun runServerLoop() {
        Log.i(TAG, "Server loop started")
        LogExporter.addLogEntry(TAG, "I", "Server loop started")
        
        try {
            while (isRunning.get() && !Thread.currentThread().isInterrupted) {
                try {
                    Log.d(TAG, "Waiting for client connection... (attempt #${connectionAttempts + 1})")
                    LogExporter.addLogEntry(TAG, "D", "Waiting for client connection (attempt #${connectionAttempts + 1})")
                    
                    connectionAttempts++
                    val connectionStartTime = System.currentTimeMillis()
                    
                    clientSocket = serverSocket?.accept()
                    val connectionTime = System.currentTimeMillis() - connectionStartTime
                    
                    Log.i(TAG, "Client connected to duplex server (connection time: ${connectionTime}ms)")
                    LogExporter.addLogEntry(TAG, "I", "Client connected to duplex server (connection time: ${connectionTime}ms, attempt #$connectionAttempts)")
                    
                    isClientConnected.set(true)
                    lastHeartbeatTime = System.currentTimeMillis()
                    
                    try {
                        outputStream = BufferedOutputStream(clientSocket?.outputStream)
                        Log.d(TAG, "Output stream initialized successfully")
                    } catch (e: IOException) {
                        Log.e(TAG, "Failed to initialize output stream", e)
                        LogExporter.addLogEntry(TAG, "E", "Failed to initialize output stream: ${e.message}")
                        continue
                    }
                    
                    // Notify handlers about client connection
                    try {
                        messageHandlers.forEach { it.onClientConnected() }
                        Log.d(TAG, "Notified ${messageHandlers.size} handlers about client connection")
                    } catch (e: Exception) {
                        Log.e(TAG, "Error notifying handlers about connection", e)
                        LogExporter.addLogEntry(TAG, "E", "Error notifying handlers about connection: ${e.message}")
                    }
                    
                    handleClientCommunication()
                    
                } catch (e: IOException) {
                    if (isRunning.get()) {
                        Log.w(TAG, "Client connection error (attempt #$connectionAttempts): ${e.message}", e)
                        LogExporter.addLogEntry(TAG, "W", "Client connection error (attempt #$connectionAttempts): ${e.message}")
                        
                        // Add delay before retrying connection
                        delay(1000)
                    }
                } catch (e: Exception) {
                    Log.e(TAG, "Unexpected error in server loop (attempt #$connectionAttempts): ${e.message}", e)
                    LogExporter.addLogEntry(TAG, "E", "Unexpected error in server loop: ${e.message}")
                } finally {
                    disconnectClient()
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Server loop error: ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Server loop error: ${e.message}")
        } finally {
            Log.i(TAG, "Server loop ended")
            LogExporter.addLogEntry(TAG, "I", "Server loop ended after $connectionAttempts connection attempts")
            cleanup()
        }
    }

    private suspend fun handleClientCommunication() {
        Log.d(TAG, "Starting client communication handling")
        LogExporter.addLogEntry(TAG, "D", "Starting client communication handling")
        
        val inputStream = BufferedInputStream(clientSocket?.inputStream)
        var messagesProcessed = 0L
        val communicationStartTime = System.currentTimeMillis()
        
        try {
            while (isRunning.get() && isClientConnected.get() && !Thread.currentThread().isInterrupted) {
                // Update heartbeat
                lastHeartbeatTime = System.currentTimeMillis()
                
                // Read message size (4 bytes)
                val sizeBytes = ByteArray(4)
                val bytesRead = inputStream.read(sizeBytes)
                
                if (bytesRead == -1) {
                    Log.d(TAG, "Client disconnected (EOF reached)")
                    LogExporter.addLogEntry(TAG, "D", "Client disconnected (EOF reached)")
                    break
                }
                
                if (bytesRead != 4) {
                    Log.e(TAG, "Invalid size header: read $bytesRead bytes, expected 4")
                    LogExporter.addLogEntry(TAG, "E", "Invalid size header: read $bytesRead bytes, expected 4")
                    break
                }
                
                val messageSize = ((sizeBytes[0].toInt() and 0xFF) shl 24) or
                                ((sizeBytes[1].toInt() and 0xFF) shl 16) or
                                ((sizeBytes[2].toInt() and 0xFF) shl 8) or
                                (sizeBytes[3].toInt() and 0xFF)
                
                if (messageSize <= 0 || messageSize > 1024 * 1024) { // Max 1MB
                    Log.e(TAG, "Invalid message size: $messageSize")
                    LogExporter.addLogEntry(TAG, "E", "Invalid message size: $messageSize")
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
                        LogExporter.addLogEntry(TAG, "E", "Unexpected EOF while reading message data")
                        break
                    }
                    totalRead += read
                }
                
                val readTime = System.currentTimeMillis() - readStartTime
                
                if (totalRead != messageSize) {
                    Log.e(TAG, "Incomplete message read: $totalRead/$messageSize (read time: ${readTime}ms)")
                    LogExporter.addLogEntry(TAG, "E", "Incomplete message read: $totalRead/$messageSize")
                    break
                }
                
                // Parse and handle message
                try {
                    val parseStartTime = System.currentTimeMillis()
                    val envelope = MessageEnvelope.parseFrom(messageData)
                    val parseTime = System.currentTimeMillis() - parseStartTime
                    
                    messagesReceived++
                    messagesProcessed++
                    
                    Log.v(TAG, "Message received: type=${envelope.type}, size=${messageSize}, parse_time=${parseTime}ms")
                    
                    val handlingStartTime = System.currentTimeMillis()
                    messageHandlers.forEach { 
                        it.onMessageReceived(envelope.type, envelope.payload.toByteArray())
                    }
                    val handlingTime = System.currentTimeMillis() - handlingStartTime
                    
                    Log.v(TAG, "Message handled: type=${envelope.type}, handlers=${messageHandlers.size}, handling_time=${handlingTime}ms")
                    
                    // Log statistics every 100 messages
                    if (messagesProcessed % 100 == 0L) {
                        val avgTime = (System.currentTimeMillis() - communicationStartTime) / messagesProcessed
                        Log.d(TAG, "Message processing stats: $messagesProcessed messages, avg_time=${avgTime}ms/msg")
                        LogExporter.addLogEntry(TAG, "D", "Processed $messagesProcessed messages (avg: ${avgTime}ms/msg)")
                    }
                    
                } catch (e: Exception) {
                    Log.e(TAG, "Error parsing/handling message: ${e.message}", e)
                    LogExporter.addLogEntry(TAG, "E", "Error parsing/handling message: ${e.message}")
                    // Continue processing other messages instead of breaking
                }
            }
            
            val totalTime = System.currentTimeMillis() - communicationStartTime
            Log.i(TAG, "Client communication ended. Stats: $messagesProcessed messages in ${totalTime}ms")
            LogExporter.addLogEntry(TAG, "I", "Client communication ended. Processed $messagesProcessed messages in ${totalTime}ms")
            
        } catch (e: IOException) {
            if (isRunning.get() && isClientConnected.get()) {
                Log.w(TAG, "Client communication error: ${e.message}", e)
                LogExporter.addLogEntry(TAG, "W", "Client communication error: ${e.message}")
            } else {
                Log.d(TAG, "Client communication ended normally")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error in client communication: ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Unexpected error in client communication: ${e.message}")
        }
    }

    fun sendMessage(type: MessageType, message: com.google.protobuf.MessageLite): Boolean {
        if (!isClientConnected.get() || outputStream == null) {
            Log.w(TAG, "Cannot send message: client not connected (type=$type)")
            LogExporter.addLogEntry(TAG, "W", "Cannot send message: client not connected (type=$type)")
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
            
            Log.v(TAG, "Message sent successfully: type=$type, size=${data.size}, time=${sendTime}ms")
            
            // Log detailed stats every 50 sent messages
            if (messagesSent % 50 == 0L) {
                LogExporter.addLogEntry(TAG, "D", "Sent $messagesSent messages total (last: type=$type, size=${data.size})")
            }
            
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error sending message (type=$type): ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Error sending message (type=$type): ${e.message}")
            disconnectClient()
            false
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error sending message (type=$type): ${e.message}", e)
            LogExporter.addLogEntry(TAG, "E", "Unexpected error sending message (type=$type): ${e.message}")
            false
        }
    }

    private fun disconnectClient() {
        if (isClientConnected.getAndSet(false)) {
            val disconnectionTime = System.currentTimeMillis()
            val sessionDuration = disconnectionTime - lastHeartbeatTime
            
            Log.i(TAG, "Disconnecting client... (session duration: ${sessionDuration}ms)")
            LogExporter.addLogEntry(TAG, "I", "Disconnecting client (session: ${sessionDuration}ms, sent: $messagesSent, received: $messagesReceived)")
            
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
            
            // Notify handlers about client disconnection
            try {
                messageHandlers.forEach { it.onClientDisconnected() }
                Log.d(TAG, "Notified ${messageHandlers.size} handlers about client disconnection")
            } catch (e: Exception) {
                Log.e(TAG, "Error notifying handlers about disconnection", e)
                LogExporter.addLogEntry(TAG, "E", "Error notifying handlers about disconnection: ${e.message}")
            }
            
            Log.i(TAG, "Client disconnected from duplex server (stats: sent=$messagesSent, received=$messagesReceived)")
            LogExporter.addLogEntry(TAG, "I", "Client disconnected from duplex server")
        }
    }

    fun stopServer() {
        if (!isRunning.getAndSet(false)) {
            Log.w(TAG, "Server is not running, stop request ignored")
            return
        }
        
        val stopTime = System.currentTimeMillis()
        val uptime = stopTime - startTime
        
        Log.i(TAG, "Stopping duplex socket server... (uptime: ${uptime}ms)")
        LogExporter.addLogEntry(TAG, "I", "Stopping server (uptime: ${uptime}ms, connections: $connectionAttempts)")
        
        try {
            serverJob?.cancel()
            Log.d(TAG, "Server job cancelled")
        } catch (e: Exception) {
            Log.w(TAG, "Error cancelling server job: ${e.message}")
        }
        
        disconnectClient()
        
        try {
            serverSocket?.close()
            Log.d(TAG, "Server socket closed")
        } catch (e: IOException) {
            Log.w(TAG, "Error closing server socket: ${e.message}")
            LogExporter.addLogEntry(TAG, "W", "Error closing server socket: ${e.message}")
        }
        
        Log.i(TAG, "Duplex socket server stopped (final stats: sent=$messagesSent, received=$messagesReceived, connections=$connectionAttempts)")
        LogExporter.addLogEntry(TAG, "I", "Duplex socket server stopped successfully")
    }

    private fun cleanup() {
        isRunning.set(false)
        disconnectClient()
        
        try {
            serverSocket?.close()
        } catch (e: IOException) { /* Ignore */ }
        
        serverSocket = null
        serverJob = null
    }

    fun addMessageHandler(handler: MessageHandler) {
        messageHandlers.add(handler)
        Log.d(TAG, "Message handler added: ${handler.javaClass.simpleName}")
    }

    fun removeMessageHandler(handler: MessageHandler) {
        messageHandlers.remove(handler)
        Log.d(TAG, "Message handler removed: ${handler.javaClass.simpleName}")
    }

    fun isConnected(): Boolean = isClientConnected.get()
    
    fun isServerRunning(): Boolean = isRunning.get()
    
    /**
     * Get detailed server status for debugging
     */
    fun getServerStatus(): String {
        val currentTime = System.currentTimeMillis()
        val uptime = if (startTime > 0) currentTime - startTime else 0
        val timeSinceLastHeartbeat = if (lastHeartbeatTime > 0) currentTime - lastHeartbeatTime else -1
        
        return buildString {
            appendLine("=== DuplexSocketServer Status ===")
            appendLine("Server Running: ${isRunning.get()}")
            appendLine("Client Connected: ${isClientConnected.get()}")
            appendLine("Socket Address: $SOCKET_ADDRESS")
            appendLine("Start Time: $startTime")
            appendLine("Uptime: ${uptime}ms")
            appendLine("Connection Attempts: $connectionAttempts")
            appendLine("Messages Sent: $messagesSent")
            appendLine("Messages Received: $messagesReceived")
            appendLine("Last Heartbeat: $lastHeartbeatTime")
            appendLine("Time Since Last Heartbeat: ${timeSinceLastHeartbeat}ms")
            appendLine("Message Handlers: ${messageHandlers.size}")
            appendLine("Server Socket: ${serverSocket != null}")
            appendLine("Client Socket: ${clientSocket != null}")
            appendLine("Output Stream: ${outputStream != null}")
            appendLine("Server Job Active: ${serverJob?.isActive ?: false}")
        }
    }
    
    /**
     * Log current server status for debugging
     */
    fun logServerStatus() {
        val status = getServerStatus()
        Log.d(TAG, status)
        LogExporter.addLogEntry(TAG, "D", "Server status requested")
        
        // Also add key metrics to LogExporter
        val currentTime = System.currentTimeMillis()
        val uptime = if (startTime > 0) currentTime - startTime else 0
        LogExporter.addLogEntry(TAG, "STATUS", "Server running=${isRunning.get()}, connected=${isClientConnected.get()}, uptime=${uptime}ms, sent=$messagesSent, received=$messagesReceived, attempts=$connectionAttempts")
    }
}