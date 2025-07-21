package io.github.chocolzs.linkura.localify.ipc

import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import java.util.concurrent.ConcurrentHashMap

class MessageRouter {
    companion object {
        private const val TAG = "MessageRouter"
    }

    private val handlers = ConcurrentHashMap<MessageType, MutableList<MessageTypeHandler>>()

    interface MessageTypeHandler {
        fun handleMessage(payload: ByteArray): Boolean
    }

    // Register handler for specific message type
    fun registerHandler(messageType: MessageType, handler: MessageTypeHandler) {
        handlers.getOrPut(messageType) { mutableListOf() }.add(handler)
        Log.d(TAG, "Handler registered for message type: $messageType")
    }

    // Unregister handler for specific message type
    fun unregisterHandler(messageType: MessageType, handler: MessageTypeHandler) {
        handlers[messageType]?.remove(handler)
        Log.d(TAG, "Handler unregistered for message type: $messageType")
    }

    // Route message to appropriate handlers
    fun routeMessage(messageType: MessageType, payload: ByteArray): Boolean {
        val typeHandlers = handlers[messageType]
        
        if (typeHandlers.isNullOrEmpty()) {
            Log.w(TAG, "No handlers found for message type: $messageType")
            return false
        }

        var handledCount = 0
        typeHandlers.forEach { handler ->
            try {
                if (handler.handleMessage(payload)) {
                    handledCount++
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error in message handler for type $messageType", e)
            }
        }

        Log.v(TAG, "Message routed to $handledCount/${typeHandlers.size} handlers for type: $messageType")
        return handledCount > 0
    }

    // Clear all handlers
    fun clearAllHandlers() {
        handlers.clear()
        Log.d(TAG, "All handlers cleared")
    }

    // Clear handlers for specific message type
    fun clearHandlers(messageType: MessageType) {
        handlers.remove(messageType)
        Log.d(TAG, "Handlers cleared for message type: $messageType")
    }

    // Get registered message types
    fun getRegisteredTypes(): Set<MessageType> = handlers.keys.toSet()
}