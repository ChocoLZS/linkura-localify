package io.github.chocolzs.linkura.localify.ipc

import android.net.LocalServerSocket
import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.IOException
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicBoolean
import kotlinx.coroutines.*

class SocketManager private constructor() {
    companion object {
        private const val TAG = "SocketManager"
        private const val SOCKET_ADDRESS = "linkura_ipc_socket"
        
        @Volatile
        private var INSTANCE: SocketManager? = null
        
        fun getInstance(): SocketManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: SocketManager().also { INSTANCE = it }
            }
        }
    }

    // Client-side socket management
    private var clientSocket: LocalSocket? = null
    private var clientOutputStream: BufferedOutputStream? = null
    private val isClientConnected = AtomicBoolean(false)
    
    // Server-side socket management
    private var serverSocket: LocalServerSocket? = null
    private val serverListenerJob = AtomicBoolean(false)
    private val messageHandlers = CopyOnWriteArrayList<MessageHandler>()
    
    interface MessageHandler {
        fun onMessageReceived(type: MessageType, payload: ByteArray)
        fun onClientConnected()
        fun onClientDisconnected()
    }

    // Client methods (for sending data)
    fun ensureClientConnected(): Boolean {
        if (isClientConnected.get() && clientSocket?.isConnected == true) {
            return true
        }
        
        return try {
            closeClient()
            clientSocket = LocalSocket().apply {
                connect(LocalSocketAddress(SOCKET_ADDRESS))
                clientOutputStream = BufferedOutputStream(outputStream)
            }
            isClientConnected.set(true)
            Log.i(TAG, "Client successfully connected to socket")
            true
        } catch (e: IOException) {
            Log.w(TAG, "Failed to connect client socket: ${e.message}")
            closeClient()
            false
        }
    }
    
    fun sendMessage(type: MessageType, message: com.google.protobuf.MessageLite): Boolean {
        if (!ensureClientConnected()) {
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
            
            clientOutputStream?.apply {
                write(sizeBytes)
                write(data)
                flush()
            }
            
            Log.v(TAG, "Message sent successfully: type=$type, size=${data.size}")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Error sending message", e)
            closeClient()
            false
        }
    }
    
    private fun closeClient() {
        try {
            clientOutputStream?.close()
        } catch (e: IOException) { /* Ignore */ }
        try {
            clientSocket?.close()
        } catch (e: IOException) { /* Ignore */ }
        clientOutputStream = null
        clientSocket = null
        isClientConnected.set(false)
    }

    // Server methods (for receiving data)
    @OptIn(DelicateCoroutinesApi::class)
    fun startServer(): Boolean {
        if (serverListenerJob.get()) {
            Log.w(TAG, "Server already running")
            return true
        }
        
        return try {
            serverSocket = LocalServerSocket(SOCKET_ADDRESS)
            serverListenerJob.set(true)
            
            GlobalScope.launch(Dispatchers.IO) {
                runServer()
            }
            
            Log.i(TAG, "Server started successfully at $SOCKET_ADDRESS")
            true
        } catch (e: IOException) {
            Log.e(TAG, "Failed to start server", e)
            false
        }
    }
    
    private suspend fun runServer() {
        try {
            while (serverListenerJob.get() && !Thread.currentThread().isInterrupted) {
                var clientSocket: LocalSocket? = null
                try {
                    clientSocket = serverSocket?.accept()
                    Log.i(TAG, "Client connected to server")
                    
                    messageHandlers.forEach { it.onClientConnected() }
                    
                    val inputStream = BufferedInputStream(clientSocket?.inputStream)
                    
                    while (serverListenerJob.get() && !Thread.currentThread().isInterrupted) {
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
                    Log.w(TAG, "Client disconnected or error", e)
                } finally {
                    try {
                        clientSocket?.close()
                    } catch (e: IOException) { /* Ignore */ }
                    
                    messageHandlers.forEach { it.onClientDisconnected() }
                    Log.i(TAG, "Client disconnected from server")
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Server error", e)
        } finally {
            try {
                serverSocket?.close()
            } catch (e: IOException) { /* Ignore */ }
            serverListenerJob.set(false)
            Log.i(TAG, "Server stopped")
        }
    }
    
    fun stopServer() {
        serverListenerJob.set(false)
        try {
            serverSocket?.close()
        } catch (e: IOException) { /* Ignore */ }
    }
    
    fun addMessageHandler(handler: MessageHandler) {
        messageHandlers.add(handler)
    }
    
    fun removeMessageHandler(handler: MessageHandler) {
        messageHandlers.remove(handler)
    }
    
    fun cleanup() {
        stopServer()
        closeClient()
        messageHandlers.clear()
    }
}