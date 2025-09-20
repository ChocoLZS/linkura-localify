package io.github.chocolzs.linkura.localify.ipc

import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import android.content.Context
import android.content.Intent

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

    private var serviceInstance: LinkuraAidlService? = null

    fun setServiceInstance(service: LinkuraAidlService) {
        serviceInstance = service
    }

    fun sendConfigUpdate(config: LinkuraConfig): Boolean {
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
                if (config.storyHideBackground != null) storyHideBackground = config.storyHideBackground
                if (config.storyHideTransition != null) storyHideTransition = config.storyHideTransition
                if (config.storyHideNonCharacter3d != null) storyHideNonCharacter3D = config.storyHideNonCharacter3d
                if (config.storyHideDof != null) storyHideDof = config.storyHideDof
                if (config.storyHideEffect != null) storyHideEffect = config.storyHideEffect
                if (config.storyNovelVocalTextDurationRate != null) storyNovelVocalTextDurationRate = config.storyNovelVocalTextDurationRate
                if (config.storyNovelNonVocalTextDurationRate != null) storyNovelNonVocalTextDurationRate = config.storyNovelNonVocalTextDurationRate
                if (config.firstPersonCameraHideHead != null) firstPersonCameraHideHead = config.firstPersonCameraHideHead
                if (config.firstPersonCameraHideHair != null) firstPersonCameraHideHair = config.firstPersonCameraHideHair
                if (config.enableMotionCaptureReplay != null) enableMotionCaptureReplay = config.enableMotionCaptureReplay
                if (config.enableInGameReplayDisplay != null) enableInGameReplayDisplay = config.enableInGameReplayDisplay
                if (config.withliveOrientation != null) withliveOrientation = config.withliveOrientation
                if (config.lockRenderTextureResolution != null) lockRenderTextureResolution = config.lockRenderTextureResolution
                if (config.renderTextureLongSide != null) renderTextureLongSide = config.renderTextureLongSide
                if (config.renderTextureShortSide != null) renderTextureShortSide = config.renderTextureShortSide
                if (config.hideCharacterBody != null) hideCharacterBody = config.hideCharacterBody
//                if (config.motionCaptureResourceUrl != null) motionCaptureResourceUrl = config.motionCaptureResourceUrl
                if (config.renderTextureAntiAliasing != null) renderTextureAntiAliasing = config.renderTextureAntiAliasing
                if (config.unlockAfter != null) unlockAfter = config.unlockAfter
                if (config.filterMotionCaptureReplay != null) filterMotionCaptureReplay = config.filterMotionCaptureReplay
                if (config.filterPlayableMotionCapture != null) filterPlayableMotionCapture = config.filterPlayableMotionCapture
                if (config.enableSetArchiveStartTime != null) enableSetArchiveStartTime = config.enableSetArchiveStartTime
                if (config.archiveStartTime != null) archiveStartTime = config.archiveStartTime
                if (config.avoidAccidentalTouch != null) avoidAccidentalTouch = config.avoidAccidentalTouch
                if (config.assetsUrlPrefix != null) assetsUrlPrefix = config.assetsUrlPrefix
                if (config.hideCharacterShadow != null) hideCharacterShadow = config.hideCharacterShadow
                if (config.hideLiveStreamSceneItemsLevel != null) hideLiveStreamSceneItemsLevel = config.hideLiveStreamSceneItemsLevel
                if (config.hideLiveStreamCharacterItems != null) hideLiveStreamCharacterItems = config.hideLiveStreamCharacterItems
            }.build()

            serviceInstance?.sendMessage(MessageType.CONFIG_UPDATE, configUpdate)
            true
        } catch (e: Exception) {
            Log.e(TAG, "Error sending config update", e)
            false
        }
    }
}