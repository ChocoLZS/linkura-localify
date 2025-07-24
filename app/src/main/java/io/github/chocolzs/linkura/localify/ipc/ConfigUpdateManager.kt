package io.github.chocolzs.linkura.localify.ipc

import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.models.LinkuraConfig

class ConfigUpdateManager private constructor() {
    companion object {
        private const val TAG = "ConfigUpdateManager"
        
        @Volatile
        private var INSTANCE: ConfigUpdateManager? = null
        
        fun getInstance(): ConfigUpdateManager {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: ConfigUpdateManager().also { INSTANCE = it }
            }
        }
    }

    private val socketServer: DuplexSocketServer by lazy { DuplexSocketServer.getInstance() }

    fun sendConfigUpdate(config: LinkuraConfig): Boolean {
        if (!socketServer.isConnected()) {
            Log.w(TAG, "Cannot send config update: no client connected")
            return false
        }

        return try {
            val configUpdate = ConfigUpdate.newBuilder().apply {
                updateType = ConfigUpdateType.FULL_UPDATE
                
                // Map LinkuraConfig fields to protobuf
                if (config.dbgMode != null) dbgMode = config.dbgMode
                if (config.enabled != null) enabled = config.enabled
                if (config.renderHighResolution != null) renderHighResolution = config.renderHighResolution
                if (config.fesArchiveUnlockTicket != null) fesArchiveUnlockTicket = config.fesArchiveUnlockTicket
                if (config.lazyInit != null) lazyInit = config.lazyInit
                if (config.replaceFont != null) replaceFont = config.replaceFont
                if (config.textTest != null) textTest = config.textTest
                if (config.dumpText != null) dumpText = config.dumpText
                if (config.enableFreeCamera != null) enableFreeCamera = config.enableFreeCamera
                if (config.targetFrameRate != null) targetFrameRate = config.targetFrameRate
                if (config.removeRenderImageCover != null) removeRenderImageCover = config.removeRenderImageCover
                if (config.avoidCharacterExit != null) avoidCharacterExit = config.avoidCharacterExit
            }.build()

            val success = socketServer.sendMessage(MessageType.CONFIG_UPDATE, configUpdate)
            if (success) {
                Log.i(TAG, "Config update sent successfully")
            } else {
                Log.e(TAG, "Failed to send config update")
            }
            success
        } catch (e: Exception) {
            Log.e(TAG, "Error sending config update", e)
            false
        }
    }

    fun isConnected(): Boolean = socketServer.isConnected()
}