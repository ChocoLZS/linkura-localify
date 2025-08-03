package io.github.chocolzs.linkura.localify.ipc

import android.net.LocalServerSocket
import android.net.LocalSocket
import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import kotlinx.coroutines.*
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.IOException
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicBoolean

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

    interface MessageHandler {
        fun onMessageReceived(type: MessageType, payload: ByteArray)
        fun onClientConnected()
        fun onClientDisconnected()
    }

    @OptIn(DelicateCoroutinesApi::class)
    fun startServer(): Boolean {
        if (isRunning.get()) {
            Log.w(TAG, "Server already running")
            return true
        }

        return try {
            serverSocket = LocalServerSocket(SOCKET_ADDRESS)
            isRunning.set(true)
            
            serverJob = GlobalScope.launch(Dispatchers.IO) {
                runServerLoop()
            }
            
            Log.i(TAG, "Duplex socket server started at $SOCKET_ADDRESS")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Failed to start server", e)
            cleanup()
            false
        }
    }

    private suspend fun runServerLoop() {
        try {
            while (isRunning.get() && !Thread.currentThread().isInterrupted) {
                try {
                    Log.d(TAG, "Waiting for client connection...")
                    clientSocket = serverSocket?.accept()
                    Log.i(TAG, "Client connected to duplex server")
                    
                    isClientConnected.set(true)
                    outputStream = BufferedOutputStream(clientSocket?.outputStream)
                    
                    // Notify handlers about client connection
                    messageHandlers.forEach { it.onClientConnected() }
                    
                    handleClientCommunication()
                    
                } catch (e: IOException) {
                    if (isRunning.get()) {
                        Log.w(TAG, "Client connection error", e)
                    }
                } finally {
                    disconnectClient()
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Server loop error", e)
        } finally {
            cleanup()
        }
    }

    private suspend fun handleClientCommunication() {
        val inputStream = BufferedInputStream(clientSocket?.inputStream)
        
        try {
            while (isRunning.get() && isClientConnected.get() && !Thread.currentThread().isInterrupted) {
                // Read message size (4 bytes)
                val sizeBytes = ByteArray(4)
                val bytesRead = inputStream.read(sizeBytes)
                if (bytesRead != 4) break
                
                val messageSize = ((sizeBytes[0].toInt() and 0xFF) shl 24) or
                                ((sizeBytes[1].toInt() and 0xFF) shl 16) or
                                ((sizeBytes[2].toInt() and 0xFF) shl 8) or
                                (sizeBytes[3].toInt() and 0xFF)
                
                if (messageSize <= 0 || messageSize > 1024 * 1024) { // Max 1MB
                    Log.e(TAG, "Invalid message size: $messageSize")
                    break
                }
                
                // Read message data
                val messageData = ByteArray(messageSize)
                var totalRead = 0
                while (totalRead < messageSize) {
                    val read = inputStream.read(messageData, totalRead, messageSize - totalRead)
                    if (read == -1) break
                    totalRead += read
                }
                
                if (totalRead != messageSize) {
                    Log.e(TAG, "Incomplete message read: $totalRead/$messageSize")
                    break
                }
                
                // Parse and handle message
                try {
                    val envelope = MessageEnvelope.parseFrom(messageData)
                    messageHandlers.forEach { 
                        it.onMessageReceived(envelope.type, envelope.payload.toByteArray())
                    }
                    Log.v(TAG, "Message received and handled: type=${envelope.type}")
                } catch (e: Exception) {
                    Log.e(TAG, "Error parsing message", e)
                }
            }
        } catch (e: IOException) {
            if (isRunning.get() && isClientConnected.get()) {
                Log.w(TAG, "Client communication error", e)
            }
        }
    }

    fun sendMessage(type: MessageType, message: com.google.protobuf.MessageLite): Boolean {
        if (!isClientConnected.get() || outputStream == null) {
            Log.w(TAG, "Cannot send message: client not connected")
            return false
        }
        
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
            
            Log.v(TAG, "Message sent successfully: type=$type, size=${data.size}")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error sending message", e)
            disconnectClient()
            false
        }
    }

    private fun disconnectClient() {
        if (isClientConnected.getAndSet(false)) {
            try {
                outputStream?.close()
            } catch (e: IOException) { /* Ignore */ }
            try {
                clientSocket?.close()
            } catch (e: IOException) { /* Ignore */ }
            
            outputStream = null
            clientSocket = null
            
            // Notify handlers about client disconnection
            messageHandlers.forEach { it.onClientDisconnected() }
            Log.i(TAG, "Client disconnected from duplex server")
        }
    }

    fun stopServer() {
        if (!isRunning.getAndSet(false)) {
            return
        }
        
        Log.i(TAG, "Stopping duplex socket server...")
        
        serverJob?.cancel()
        disconnectClient()
        
        try {
            serverSocket?.close()
        } catch (e: IOException) {
            Log.w(TAG, "Error closing server socket", e)
        }
        
        Log.i(TAG, "Duplex socket server stopped")
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
}