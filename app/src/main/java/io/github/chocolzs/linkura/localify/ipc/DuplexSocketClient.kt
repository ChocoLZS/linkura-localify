package io.github.chocolzs.linkura.localify.ipc

import android.net.LocalSocket
import android.net.LocalSocketAddress
import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import kotlinx.coroutines.*
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.IOException
import java.util.concurrent.CopyOnWriteArrayList
import java.util.concurrent.atomic.AtomicBoolean

class DuplexSocketClient private constructor() {
    companion object {
        private const val TAG = "DuplexSocketClient"
        private const val SOCKET_ADDRESS = "linkura_duplex_socket"
        private const val SOCKET_ADDRESS_FALLBACK = "@linkura_duplex_socket_abs"
        private const val RECONNECT_INTERVAL_MS = 2000L
        private const val CONNECTION_TIMEOUT_MS = 5000L
        
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
            return true
        }

        isRunning.set(true)
        clientJob = GlobalScope.launch(Dispatchers.IO) {
            runClientLoop()
        }
        
        Log.i(TAG, "Duplex socket client started")
        return true
    }

    private suspend fun runClientLoop() {
        while (isRunning.get() && !Thread.currentThread().isInterrupted) {
            try {
                if (connectToServer()) {
                    handleServerCommunication()
                } else {
                    // Notify handlers about connection failure
                    messageHandlers.forEach { it.onConnectionFailed() }
                }
            } catch (e: Exception) {
                Log.w(TAG, "Client loop error", e)
            } finally {
                disconnect()
            }

            if (isRunning.get()) {
                Log.d(TAG, "Reconnecting in ${RECONNECT_INTERVAL_MS}ms...")
                delay(RECONNECT_INTERVAL_MS)
            }
        }
        
        Log.i(TAG, "Client loop ended")
    }

    private fun connectToServer(): Boolean {
        // Try to connect with filesystem namespace first
        var success = tryConnectToServer(SOCKET_ADDRESS, false)
        
        // If failed, try with abstract namespace as fallback
        if (!success) {
            Log.w(TAG, "Failed to connect with filesystem namespace, trying abstract namespace")
            success = tryConnectToServer(SOCKET_ADDRESS_FALLBACK, true)
        }
        
        return success
    }
    
    private fun tryConnectToServer(address: String, abstractNamespace: Boolean): Boolean {
        try {
            disconnect()
            
            Log.d(TAG, "Connecting to server at $address (abstract: $abstractNamespace)...")
            clientSocket = LocalSocket().apply {
                // Set connection timeout
                soTimeout = CONNECTION_TIMEOUT_MS.toInt()
                connect(LocalSocketAddress(address))
            }
            outputStream = BufferedOutputStream(clientSocket?.outputStream)
            
            isConnected.set(true)
            Log.i(TAG, "Successfully connected to duplex server at $address")
            
            // Notify handlers about successful connection
            messageHandlers.forEach { it.onConnected() }
            return true
            
        } catch (e: IOException) {
            Log.w(TAG, "Failed to connect to server at $address (abstract: $abstractNamespace): ${e.message}")
            disconnect()
            return false
        } catch (e: Exception) {
            Log.e(TAG, "Unexpected error connecting to server at $address", e)
            disconnect()
            return false
        }
    }

    private suspend fun handleServerCommunication() {
        val inputStream = BufferedInputStream(clientSocket?.inputStream)
        
        try {
            while (isRunning.get() && isConnected.get() && !Thread.currentThread().isInterrupted) {
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
            if (isRunning.get() && isConnected.get()) {
                Log.w(TAG, "Server communication error", e)
            }
        }
    }

    fun sendMessage(type: MessageType, message: com.google.protobuf.MessageLite): Boolean {
        if (!isConnected.get() || outputStream == null) {
            Log.w(TAG, "Cannot send message: not connected to server")
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
            disconnect()
            false
        }
    }

    fun stopClient() {
        if (!isRunning.getAndSet(false)) {
            return
        }
        
        Log.i(TAG, "Stopping duplex socket client...")
        
        clientJob?.cancel()
        disconnect()
        
        Log.i(TAG, "Duplex socket client stopped")
    }

    private fun disconnect() {
        if (isConnected.getAndSet(false)) {
            try {
                outputStream?.close()
            } catch (e: IOException) { /* Ignore */ }
            try {
                clientSocket?.close()
            } catch (e: IOException) { /* Ignore */ }
            
            outputStream = null
            clientSocket = null
            
            // Notify handlers about disconnection
            messageHandlers.forEach { it.onDisconnected() }
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
}