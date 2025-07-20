package io.github.chocolzs.linkura.localify.ipc

import android.app.Service
import android.content.Intent
import android.os.IBinder
import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.MessageType

abstract class SocketService : Service(), SocketManager.MessageHandler {
    companion object {
        private const val TAG = "SocketService"
    }
    
    protected val socketManager: SocketManager by lazy { SocketManager.getInstance() }
    private var isServerMode = false
    
    override fun onBind(intent: Intent?): IBinder? = null
    
    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "${this.javaClass.simpleName} onCreate")
        
        // Services that extend this class should call setupAsServer() or setupAsClient()
        onSocketServiceCreate()
    }
    
    override fun onDestroy() {
        super.onDestroy()
        Log.d(TAG, "${this.javaClass.simpleName} onDestroy")
        
        socketManager.removeMessageHandler(this)
        if (isServerMode) {
            socketManager.stopServer()
        }
        
        onSocketServiceDestroy()
    }
    
    protected fun setupAsServer(): Boolean {
        isServerMode = true
        socketManager.addMessageHandler(this)
        return socketManager.startServer()
    }
    
    protected fun setupAsClient(): Boolean {
        isServerMode = false
        return socketManager.ensureClientConnected()
    }
    
    protected fun sendMessage(type: MessageType, message: com.google.protobuf.MessageLite): Boolean {
        return socketManager.sendMessage(type, message)
    }
    
    // Abstract methods for subclasses to implement
    abstract fun onSocketServiceCreate()
    abstract fun onSocketServiceDestroy()
    
    // SocketManager.MessageHandler implementation
    override fun onMessageReceived(type: MessageType, payload: ByteArray) {
        handleReceivedMessage(type, payload)
    }
    
    override fun onClientConnected() {
        onSocketClientConnected()
    }
    
    override fun onClientDisconnected() {
        onSocketClientDisconnected()
    }
    
    // Methods for subclasses to override
    protected open fun handleReceivedMessage(type: MessageType, payload: ByteArray) {
        Log.d(TAG, "${this.javaClass.simpleName} received message: type=$type, size=${payload.size}")
    }
    
    protected open fun onSocketClientConnected() {
        Log.d(TAG, "${this.javaClass.simpleName} client connected")
    }
    
    protected open fun onSocketClientDisconnected() {
        Log.d(TAG, "${this.javaClass.simpleName} client disconnected")
    }
}