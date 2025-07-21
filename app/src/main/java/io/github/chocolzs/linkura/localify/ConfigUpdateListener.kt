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
    fun onTargetFpsChanged(s: CharSequence, start: Int, before: Int, count: Int)
    fun onDumpTextChanged(value: Boolean)

    fun onPTransRemoteZipUrlChanged(s: CharSequence, start: Int, before: Int, count: Int)
    fun mainPageAssetsViewDataUpdate(downloadAbleState: Boolean? = null,
                                     downloadProgressState: Float? = null,
                                     localResourceVersionState: String? = null,
                                     errorString: String? = null,
                                     localAPIResourceVersion: String? = null)
    fun mainUIConfirmStatUpdate(isShow: Boolean? = null, title: String? = null,
                                content: String? = null,
                                onConfirm: (() -> Unit)? = { mainUIConfirmStatUpdate(isShow = false) },
                                onCancel: (() -> Unit)? = { mainUIConfirmStatUpdate(isShow = false) })
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
    fun sendConfigUpdate(config: LinkuraConfig, isPartial: Boolean = false, changedFields: Set<String> = emptySet()) {
        val configUpdateManager = ConfigUpdateManager.getInstance()
        
        try {
            val success = if (isPartial && changedFields.isNotEmpty()) {
                // Send partial update using the convenient helper method
                val updates = changedFields.mapNotNull { field ->
                    when (field) {
                        "enabled" -> "enabled" to config.enabled
                        "renderHighResolution" -> "renderHighResolution" to config.renderHighResolution
                        "fesArchiveUnlockTicket" -> "fesArchiveUnlockTicket" to config.fesArchiveUnlockTicket
                        "lazyInit" -> "lazyInit" to config.lazyInit
                        "replaceFont" -> "replaceFont" to config.replaceFont
                        "textTest" -> "textTest" to config.textTest
                        "dumpText" -> "dumpText" to config.dumpText
                        "enableFreeCamera" -> "enableFreeCamera" to config.enableFreeCamera
                        "targetFrameRate" -> "targetFrameRate" to config.targetFrameRate
                        "dbgMode" -> "dbgMode" to config.dbgMode
                        else -> null
                    }
                }.toTypedArray()
                
                configUpdateManager.sendPartialConfigUpdate(*updates)
            } else {
                // Send full configuration update
                configUpdateManager.sendConfigUpdate(config)
            }
            
            if (success) {
                Log.i(TAG, "Config hot-reload sent successfully via duplex socket: ${if (isPartial) "partial" else "full"} update")
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
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("enabled"))
        pushKeyEvent(KeyEvent(1145, 29))
    }

    override fun onRenderHighResolutionChanged(value: Boolean) {
        config.renderHighResolution = value
        saveConfig()
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("renderHighResolution"))
    }

    override fun onFesArchiveUnlockTicketChanged(value: Boolean) {
        config.fesArchiveUnlockTicket = value
        saveConfig()
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("fesArchiveUnlockTicket"))
    }

    override fun onForceExportResourceChanged(value: Boolean) {
        config.forceExportResource = value
        saveConfig()
        pushKeyEvent(KeyEvent(1145, 30))
    }

    override fun onReplaceFontChanged(value: Boolean) {
        config.replaceFont = value
        saveConfig()
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("replaceFont"))
        pushKeyEvent(KeyEvent(1145, 30))
    }

    override fun onLazyInitChanged(value: Boolean) {
        config.lazyInit = value
        saveConfig()
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("lazyInit"))
    }

    override fun onTextTestChanged(value: Boolean) {
        config.textTest = value
        saveConfig()
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("textTest"))
    }

    override fun onDumpTextChanged(value: Boolean) {
        config.dumpText = value
        saveConfig()
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("dumpText"))
    }

    override fun onEnableFreeCameraChanged(value: Boolean) {
        config.enableFreeCamera = value
        saveConfig()
        sendConfigUpdate(config, isPartial = true, changedFields = setOf("enableFreeCamera"))
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
            sendConfigUpdate(config, isPartial = true, changedFields = setOf("targetFrameRate"))
        }
        catch (e: Exception) {
            return
        }
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

    override fun mainUIConfirmStatUpdate(isShow: Boolean?, title: String?, content: String?,
        onConfirm: (() -> Unit)?, onCancel: (() -> Unit)?
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
