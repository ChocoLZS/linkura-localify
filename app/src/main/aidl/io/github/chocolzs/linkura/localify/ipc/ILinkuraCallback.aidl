package io.github.chocolzs.linkura.localify.ipc;

/**
 * Callback interface for receiving messages from the Linkura service
 * This enables bidirectional communication
 */
interface ILinkuraCallback {
    
    /**
     * Called when a message is received from the server
     * @param messageType The type of message (see MessageType enum)
     * @param payload The message payload as byte array
     */
    void onMessageReceived(int messageType, in byte[] payload);
    
    /**
     * Called when client successfully connects to the server
     */
    void onConnected();
    
    /**
     * Called when client disconnects from the server
     */
    void onDisconnected();
    
    /**
     * Called when connection fails
     */
    void onConnectionFailed();
}