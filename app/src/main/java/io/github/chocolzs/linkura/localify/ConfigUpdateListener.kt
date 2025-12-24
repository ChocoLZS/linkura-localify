package io.github.chocolzs.linkura.localify

import android.util.Log
import android.view.KeyEvent
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.ProgramConfig
import io.github.chocolzs.linkura.localify.models.ProgramConfigViewModel
import io.github.chocolzs.linkura.localify.models.ProgramConfigViewModelFactory
import io.github.chocolzs.linkura.localify.ipc.ConfigUpdateManager
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.runBlocking


interface ConfigListener {
    fun onEnabledChanged(value: Boolean)
    fun onRenderHighResolutionChanged(value: Boolean)
    fun onFesArchiveUnlockTicketChanged(value: Boolean)
    fun onForceExportResourceChanged(value: Boolean)
    fun onTextTestChanged(value: Boolean)
    fun onReplaceFontChanged(value: Boolean)
    fun onLazyInitChanged(value: Boolean)
    fun onEnableFreeCameraChanged(value: Boolean)
    fun onMemorizeFreeCameraPosChanged(value: Boolean)
    fun onTargetFpsChanged(s: CharSequence, start: Int, before: Int, count: Int)
    fun onDumpTextChanged(value: Boolean)
    fun onRemoveRenderImageCoverChanged(value: Boolean)
    fun onAvoidCharacterExitChanged(value: Boolean)
    fun onStoryHideBackgroundChanged(value: Boolean)
    fun onStoryHideTransitionChanged(value: Boolean)
    fun onStoryHideNonCharacter3dChanged(value: Boolean)
    fun onStoryHideDofChanged(value: Boolean)
    fun onStoryHideEffectChanged(value: Boolean)
    fun onStoryNovelVocalTextDurationRateChanged(value: Float)
    fun onStoryNovelNonVocalTextDurationRateChanged(value: Float)
    fun onStoryReplaceContentChanged(value: String)
    fun onFirstPersonCameraHideHeadChanged(value: Boolean)
    fun onFirstPersonCameraHideHairChanged(value: Boolean)
    fun onEnableMotionCaptureReplayChanged(value: Boolean)
    fun onEnableInGameReplayDisplayChanged(value: Boolean)
    fun onMotionCaptureResourceUrlChanged(value: String)
    fun onWithliveOrientationChanged(value: Int)
    fun onLockRenderTextureResolutionChanged(value: Boolean)
    fun onRenderTextureResolutionChanged(longSide: Int, shortSide: Int)
    fun onHideCharacterBodyChanged(value: Boolean)
    fun onRenderTextureAntiAliasingChanged(value: Int)
    fun onUnlockAfterChanged(value: Boolean)
    fun onCameraMovementSensitivityChanged(value: Float)
    fun onCameraVerticalSensitivityChanged(value: Float)
    fun onCameraFovSensitivityChanged(value: Float)
    fun onCameraRotationSensitivityChanged(value: Float)
    fun onEnableLegacyCompatibilityChanged(value: Boolean)
    fun onFilterMotionCaptureReplayChanged(value: Boolean)
    fun onFilterPlayableMotionCaptureChanged(value: Boolean)
    fun onEnableSetArchiveStartTimeChanged(value: Boolean)
    fun onArchiveStartTimeChanged(value: Int)
    fun onAvoidAccidentalTouchChanged(value: Boolean)
    fun onAssetsUrlPrefixChanged(value: String)
    fun onHideCharacterShadowChanged(value: Boolean)
    fun onHideLiveStreamSceneItemsLevel(value: Int)
    fun onHideLiveStreamCharacterItems(value: Boolean)
    fun onEnableInGameOverlayToolbar(value: Boolean)
    fun onLocaleCodeChanged(value: String)

    fun onPUseRemoteAssetsChanged(value: Boolean)
    fun onPCleanLocalAssetsChanged(value: Boolean)
    fun onPDelRemoteAfterUpdateChanged(value: Boolean)
    fun onPTransRemoteZipUrlChanged(s: CharSequence, start: Int, before: Int, count: Int)
    fun mainPageAssetsViewDataUpdate(downloadAbleState: Boolean? = null,
                                     downloadProgressState: Float? = null,
                                     localResourceVersionState: String? = null,
                                     errorString: String? = null,
                                     localAPIResourceVersion: String? = null)
    fun onPUsePluginBuiltInAssetsChanged(value: Boolean)
    fun onPCheckAppUpdateChanged(value: Boolean)
    fun onPUseAPIAssetsChanged(value: Boolean)
    fun onPUseAPIAssetsURLChanged(s: CharSequence, start: Int, before: Int, count: Int)
    fun mainUIConfirmStatUpdate(isShow: Boolean? = null, title: String? = null,
                                content: String? = null,
                                onConfirm: (() -> Unit)? = { mainUIConfirmStatUpdate(isShow = false) },
                                onCancel: (() -> Unit)? = { mainUIConfirmStatUpdate(isShow = false) },
                                confirmText: String? = null,
                                cancelText: String? = null)
}

class UserConfigViewModelFactory(private val initialValue: LinkuraConfig) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(UserConfigViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return UserConfigViewModel(initialValue) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}

class UserConfigViewModel(initValue: LinkuraConfig) : ViewModel() {
    val configState = MutableStateFlow(initValue)
    val config: StateFlow<LinkuraConfig> = configState.asStateFlow()
}


interface ConfigUpdateListener: ConfigListener, IHasConfigItems {
    var factory: UserConfigViewModelFactory
    var viewModel: UserConfigViewModel

    var programConfigFactory: ProgramConfigViewModelFactory
    var programConfigViewModel: ProgramConfigViewModel

    fun pushKeyEvent(event: KeyEvent): Boolean
    fun checkConfigAndUpdateView() {}  // do nothing
    // fun saveConfig()
    fun saveProgramConfig()
    
    // Hot-reload configuration management using new duplex socket system
    fun sendConfigUpdate(config: LinkuraConfig) {
        val configUpdateManager = ConfigUpdateManager.getInstance()
        
        try {
            val success = configUpdateManager.sendConfigUpdate(config)
            
            if (success) {
                Log.i(TAG, "Config hot-reload sent successfully via duplex socket: ${"full"} update")
            } else {
                Log.w(TAG, "Failed to send config hot-reload via duplex socket")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending config hot-reload via duplex socket", e)
        }
    }


    override fun onEnabledChanged(value: Boolean) {
        config.enabled = value
        saveConfig()
        sendConfigUpdate(config)
        pushKeyEvent(KeyEvent(1145, 29))
    }

    override fun onRenderHighResolutionChanged(value: Boolean) {
        config.renderHighResolution = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onFesArchiveUnlockTicketChanged(value: Boolean) {
        config.fesArchiveUnlockTicket = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onForceExportResourceChanged(value: Boolean) {
        config.forceExportResource = value
        saveConfig()
        pushKeyEvent(KeyEvent(1145, 30))
    }

    override fun onReplaceFontChanged(value: Boolean) {
        config.replaceFont = value
        saveConfig()
        sendConfigUpdate(config)
        pushKeyEvent(KeyEvent(1145, 30))
    }

    override fun onLazyInitChanged(value: Boolean) {
        config.lazyInit = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onTextTestChanged(value: Boolean) {
        config.textTest = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onDumpTextChanged(value: Boolean) {
        config.dumpText = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onEnableFreeCameraChanged(value: Boolean) {
        config.enableFreeCamera = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onMemorizeFreeCameraPosChanged(value: Boolean) {
        config.memorizeFreeCameraPos = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onTargetFpsChanged(s: CharSequence, start: Int, before: Int, count: Int) {
        try {
            val valueStr = s.toString()

            val value = if (valueStr == "") {
                0
            } else {
                valueStr.toInt()
            }
            config.targetFrameRate = value
            saveConfig()
            sendConfigUpdate(config)
        }
        catch (e: Exception) {
            return
        }
    }

    override fun onRemoveRenderImageCoverChanged(value: Boolean) {
        config.removeRenderImageCover = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onAvoidCharacterExitChanged(value: Boolean) {
        config.avoidCharacterExit = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onStoryHideBackgroundChanged(value: Boolean) {
        config.storyHideBackground = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onStoryHideTransitionChanged(value: Boolean) {
        config.storyHideTransition = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onStoryHideNonCharacter3dChanged(value: Boolean) {
        config.storyHideNonCharacter3d = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onStoryHideDofChanged(value: Boolean) {
        config.storyHideDof = value
        saveConfig()
        sendConfigUpdate(config)
    }
    override fun onStoryHideEffectChanged(value: Boolean) {
        config.storyHideEffect = value
        saveConfig()
        sendConfigUpdate(config)
    }
    override fun onStoryNovelVocalTextDurationRateChanged(value: Float) {
        config.storyNovelVocalTextDurationRate = value
        saveConfig()
        sendConfigUpdate(config)
    }
    override fun onStoryNovelNonVocalTextDurationRateChanged(value: Float) {
        config.storyNovelNonVocalTextDurationRate = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onStoryReplaceContentChanged(value: String) {
        config.storyReplaceContent = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onFirstPersonCameraHideHairChanged(value: Boolean) {
        config.firstPersonCameraHideHair = value
        saveConfig()
        sendConfigUpdate(config)
    }
    override fun onFirstPersonCameraHideHeadChanged(value: Boolean) {
        config.firstPersonCameraHideHead = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onEnableMotionCaptureReplayChanged(value: Boolean) {
        config.enableMotionCaptureReplay = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onEnableInGameReplayDisplayChanged(value: Boolean) {
        config.enableInGameReplayDisplay = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onMotionCaptureResourceUrlChanged(value: String) {
        config.motionCaptureResourceUrl = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onWithliveOrientationChanged(value: Int) {
        config.withliveOrientation = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onLockRenderTextureResolutionChanged(value: Boolean) {
        config.lockRenderTextureResolution = value
        saveConfig()
        sendConfigUpdate(config)
    }
    override fun onRenderTextureResolutionChanged(longSide: Int, shortSide: Int) {
        config.renderTextureLongSide = longSide
        config.renderTextureShortSide = shortSide
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onHideCharacterBodyChanged(value: Boolean) {
        config.hideCharacterBody = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onRenderTextureAntiAliasingChanged(value: Int) {
        config.renderTextureAntiAliasing = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onUnlockAfterChanged(value: Boolean) {
        config.unlockAfter = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onCameraMovementSensitivityChanged(value: Float) {
        config.cameraMovementSensitivity = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onCameraVerticalSensitivityChanged(value: Float) {
        config.cameraVerticalSensitivity = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onCameraFovSensitivityChanged(value: Float) {
        config.cameraFovSensitivity = value
        saveConfig()
        sendConfigUpdate(config)
    }

    /**
     * No Hot Reload
     */
    override fun onEnableLegacyCompatibilityChanged(value: Boolean) {
        config.enableLegacyCompatibility = value
        saveConfig()
    }

    override fun onCameraRotationSensitivityChanged(value: Float) {
        config.cameraRotationSensitivity = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onFilterMotionCaptureReplayChanged(value: Boolean) {
        config.filterMotionCaptureReplay = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onFilterPlayableMotionCaptureChanged(value: Boolean) {
        config.filterPlayableMotionCapture = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onEnableSetArchiveStartTimeChanged(value: Boolean) {
        config.enableSetArchiveStartTime = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onArchiveStartTimeChanged(value: Int) {
        config.archiveStartTime = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onAvoidAccidentalTouchChanged(value: Boolean) {
        config.avoidAccidentalTouch = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onAssetsUrlPrefixChanged(value: String) {
        config.assetsUrlPrefix = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onHideCharacterShadowChanged(value: Boolean) {
        config.hideCharacterShadow = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onHideLiveStreamSceneItemsLevel(value: Int) {
        config.hideLiveStreamSceneItemsLevel = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onHideLiveStreamCharacterItems(value: Boolean) {
        config.hideLiveStreamCharacterItems = value
        saveConfig()
        sendConfigUpdate(config)
    }

    override fun onEnableInGameOverlayToolbar(value: Boolean) {
        config.enableInGameOverlayToolbar = value
        saveConfig()
        // do not send update, useless
    }
    override fun onLocaleCodeChanged(value: String) {
        config.localeCode = value
        saveConfig()
    }

    override fun onPUsePluginBuiltInAssetsChanged(value: Boolean) {
        programConfig.usePluginBuiltInAssets = value
        if (value) {
            programConfig.cleanLocalAssets = false
            programConfig.useRemoteAssets = false
            programConfig.useAPIAssets = false
        }
        saveProgramConfig()
    }

    override fun onPCheckAppUpdateChanged(value: Boolean) {
        programConfig.checkAppUpdate = value
        saveProgramConfig()
    }

    override fun onPUseRemoteAssetsChanged(value: Boolean) {
        programConfig.useRemoteAssets = value
        if (value) {
            programConfig.usePluginBuiltInAssets = false
            programConfig.cleanLocalAssets = false
            programConfig.useAPIAssets = false
        }
        saveProgramConfig()
    }

    override fun onPCleanLocalAssetsChanged(value: Boolean) {
        programConfig.cleanLocalAssets = value
        if (value) {
            programConfig.useRemoteAssets = false
            programConfig.useAPIAssets = false
            programConfig.usePluginBuiltInAssets = false
        }
        saveProgramConfig()
    }

    override fun onPDelRemoteAfterUpdateChanged(value: Boolean) {
        programConfig.delRemoteAfterUpdate = value
        saveProgramConfig()
    }
    override fun onPTransRemoteZipUrlChanged(s: CharSequence, start: Int, before: Int, count: Int) {
        programConfig.transRemoteZipUrl = s.toString()
        saveProgramConfig()
    }

    override fun mainPageAssetsViewDataUpdate(downloadAbleState: Boolean?, downloadProgressState: Float?,
                                              localResourceVersionState: String?, errorString: String?,
                                              localAPIResourceVersion: String?) {
        downloadAbleState?.let { programConfigViewModel.downloadAbleState.value = it }
        downloadProgressState?.let{ programConfigViewModel.downloadProgressState.value = it }
        localResourceVersionState?.let{ programConfigViewModel.localResourceVersionState.value = it }
        errorString?.let{ programConfigViewModel.errorStringState.value = it }
        localAPIResourceVersion?.let{ programConfigViewModel.localAPIResourceVersionState.value = it }
    }
    override fun onPUseAPIAssetsChanged(value: Boolean) {
        programConfig.useAPIAssets = value
        if (value) {
            programConfig.usePluginBuiltInAssets = false
            programConfig.useRemoteAssets = false
            programConfig.cleanLocalAssets = false
        }
        saveProgramConfig()
    }

    override fun onPUseAPIAssetsURLChanged(s: CharSequence, start: Int, before: Int, count: Int) {
        programConfig.useAPIAssetsURL = s.toString()
        saveProgramConfig()
    }
    override fun mainUIConfirmStatUpdate(isShow: Boolean?, title: String?, content: String?,
        onConfirm: (() -> Unit)?, onCancel: (() -> Unit)?,
        confirmText: String?, cancelText: String?
    ) {
        val orig = programConfigViewModel.mainUIConfirmState.value
        isShow?.let {
            if (orig.isShow && it) {
                Log.e(TAG, "Duplicate mainUIConfirmStat")
            }
            orig.isShow = it
        }
        title?.let { orig.title = it }
        content?.let { orig.content = it }
        confirmText?.let { orig.confirmText = it }
        cancelText?.let { orig.cancelText = it }
        onConfirm?.let { orig.onConfirm = {
            try {
                it()
            }
            finally {
                mainUIConfirmStatUpdate(isShow = false)
            }
        } }
        onCancel?.let { orig.onCancel = {
            try {
                it()
            }
            finally {
                mainUIConfirmStatUpdate(isShow = false)
            }
        } }
        orig.p = false
        programConfigViewModel.mainUIConfirmState.value = orig.copy(p = true)
    }
}
