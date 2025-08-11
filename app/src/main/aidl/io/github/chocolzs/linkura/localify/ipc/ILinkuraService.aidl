package io.github.chocolzs.linkura.localify.ipc;

import io.github.chocolzs.linkura.localify.ipc.ILinkuraCallback;

/**
 * AIDL interface for Linkura IPC communication
 * Server side: Java application
 * Client side: Xposed module
 */
interface ILinkuraService {
    
    /**
     * Register a callback for receiving messages from the server
     * @param callback The callback interface
     */
    void registerCallback(ILinkuraCallback callback);
    
    /**
     * Unregister a callback
     * @param callback The callback interface to remove
     */
    void unregisterCallback(ILinkuraCallback callback);
    
    /**
     * Send a message to the server
     * @param messageType The type of message (see MessageType enum)
     * @param payload The message payload as byte array
     * @return true if message was sent successfully
     */
    boolean sendMessage(int messageType, in byte[] payload);
    
    /**
     * Get server status information
     * @return Status string for debugging
     */
    String getServerStatus();
    
    /**
     * Check if server is running and ready
     * @return true if server is ready for communication
     */
    boolean isReady();
    
    /**
     * Get the number of active clients
     * @return Number of connected clients
     */
    int getClientCount();
}