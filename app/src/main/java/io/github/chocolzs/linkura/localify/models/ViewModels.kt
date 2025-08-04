package io.github.chocolzs.linkura.localify.models

import androidx.lifecycle.ViewModel
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.mutableStateOf
import androidx.lifecycle.ViewModelProvider
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

open class CollapsibleBoxViewModel(initiallyBreastExpanded: Boolean = false) : ViewModel() {
    open var expanded by mutableStateOf(initiallyBreastExpanded)
}

class BreastCollapsibleBoxViewModel(initiallyBreastExpanded: Boolean = false) : CollapsibleBoxViewModel(initiallyBreastExpanded) {
    override var expanded by mutableStateOf(initiallyBreastExpanded)
}

class ResourceCollapsibleBoxViewModel(initiallyBreastExpanded: Boolean = false) : CollapsibleBoxViewModel(initiallyBreastExpanded) {
    override var expanded by mutableStateOf(initiallyBreastExpanded)
}

class FirstPersonCameraCollapsibleBoxViewModel(initiallyExpanded: Boolean = false) : CollapsibleBoxViewModel(initiallyExpanded) {
    override var expanded by mutableStateOf(initiallyExpanded)
}

class ReplaySettingsCollapsibleBoxViewModel(initiallyExpanded: Boolean = true) : CollapsibleBoxViewModel(initiallyExpanded) {
    override var expanded by mutableStateOf(initiallyExpanded)
}

class BreastCollapsibleBoxViewModelFactory(private val initiallyExpanded: Boolean) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(BreastCollapsibleBoxViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return BreastCollapsibleBoxViewModel(initiallyExpanded) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}

class ResourceCollapsibleBoxViewModelFactory(private val initiallyExpanded: Boolean) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(ResourceCollapsibleBoxViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return ResourceCollapsibleBoxViewModel(initiallyExpanded) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}

class FirstPersonCameraCollapsibleBoxViewModelFactory(private val initiallyExpanded: Boolean) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(FirstPersonCameraCollapsibleBoxViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return FirstPersonCameraCollapsibleBoxViewModel(initiallyExpanded) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}

class ReplaySettingsCollapsibleBoxViewModelFactory(private val initiallyExpanded: Boolean) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(ReplaySettingsCollapsibleBoxViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return ReplaySettingsCollapsibleBoxViewModel(initiallyExpanded) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}


class ProgramConfigViewModelFactory(private val initialValue: ProgramConfig,
                                    private val localResourceVersion: String) : ViewModelProvider.Factory {
    override fun <T : ViewModel> create(modelClass: Class<T>): T {
        if (modelClass.isAssignableFrom(ProgramConfigViewModel::class.java)) {
            @Suppress("UNCHECKED_CAST")
            return ProgramConfigViewModel(initialValue, localResourceVersion) as T
        }
        throw IllegalArgumentException("Unknown ViewModel class")
    }
}

data class ConfirmStateModel(
    var isShow: Boolean = false,
    var title: String = "GakuConfirm Title",
    var content: String = "GakuConfirm Content",
    var onConfirm: () -> Unit = {},
    var onCancel: () -> Unit = {},
    var p: Boolean = false
)

class ProgramConfigViewModel(initValue: ProgramConfig, initLocalResourceVersion: String) : ViewModel() {
    val configState = MutableStateFlow(initValue)
    val config: StateFlow<ProgramConfig> = configState.asStateFlow()

    val downloadProgressState = MutableStateFlow(-1f)
    val downloadProgress: StateFlow<Float> = downloadProgressState.asStateFlow()

    val downloadAbleState = MutableStateFlow(true)
    val downloadAble: StateFlow<Boolean> = downloadAbleState.asStateFlow()

    val localResourceVersionState = MutableStateFlow(initLocalResourceVersion)
    val localResourceVersion: StateFlow<String> = localResourceVersionState.asStateFlow()

    val localAPIResourceVersionState = MutableStateFlow(initLocalResourceVersion)
    val localAPIResourceVersion: StateFlow<String> = localAPIResourceVersionState.asStateFlow()

    val errorStringState = MutableStateFlow("")
    val errorString: StateFlow<String> = errorStringState.asStateFlow()

    val mainUIConfirmState = MutableStateFlow(ConfirmStateModel())
    val mainUIConfirm: StateFlow<ConfirmStateModel> = mainUIConfirmState.asStateFlow()
}
