package io.github.chocolzs.linkura.localify

import android.annotation.SuppressLint
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.util.Log
import android.view.KeyEvent
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.collectAsState
import androidx.lifecycle.ViewModelProvider
import io.github.chocolzs.linkura.localify.hookUtils.FileHotUpdater
import io.github.chocolzs.linkura.localify.hookUtils.FilesChecker
import io.github.chocolzs.linkura.localify.hookUtils.MainKeyEventDispatcher
import io.github.chocolzs.linkura.localify.ipc.DuplexSocketServer
import io.github.chocolzs.linkura.localify.mainUtils.RemoteAPIFilesChecker
import io.github.chocolzs.linkura.localify.mainUtils.ShizukuApi
import io.github.chocolzs.linkura.localify.mainUtils.json
import io.github.chocolzs.linkura.localify.models.ConfirmStateModel
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.ProgramConfig
import io.github.chocolzs.linkura.localify.models.ProgramConfigViewModel
import io.github.chocolzs.linkura.localify.models.ProgramConfigViewModelFactory
import io.github.chocolzs.linkura.localify.ui.overlay.CameraOverlayService
import io.github.chocolzs.linkura.localify.ui.overlay.CameraOverlayService.Companion
import io.github.chocolzs.linkura.localify.ui.pages.MainUI
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.serialization.encodeToString
import java.io.File


class MainActivity : ComponentActivity(), ConfigUpdateListener, IConfigurableActivity<MainActivity> {
    override lateinit var config: LinkuraConfig
    override lateinit var programConfig: ProgramConfig

    override lateinit var factory: UserConfigViewModelFactory
    override lateinit var viewModel: UserConfigViewModel

    override lateinit var programConfigFactory: ProgramConfigViewModelFactory
    override lateinit var programConfigViewModel: ProgramConfigViewModel

    private val socketServer: DuplexSocketServer by lazy { DuplexSocketServer.getInstance() }


    private fun showToast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    fun gotoPatchActivity() {
        val intent = Intent(this, PatchActivity::class.java)
        startActivity(intent)
    }

    override fun saveConfig() {
        try {
            config.pf = false
            viewModel.configState.value = config.copy( pf = true )  // 更新 UI
        }
        catch (e: RuntimeException) {
            Log.d(TAG, e.toString())
        }
        val configFile = File(filesDir, "l4-config.json")
        configFile.writeText(json.encodeToString(config))
    }

    override fun saveProgramConfig() {
        try {
            programConfig.p = false
            programConfigViewModel.configState.value = programConfig.copy( p = true )  // 更新 UI
        }
        catch (e: RuntimeException) {
            Log.d(TAG, e.toString())
        }
        val configFile = File(filesDir, "localify-config.json")
        configFile.writeText(json.encodeToString(programConfig))
    }

    fun getVersion(): List<String> {
        var versionText = ""
        var resVersionText = "unknown"

        try {
            val stream = assets.open("${FilesChecker.localizationFilesDir}/version.txt")
            resVersionText = FilesChecker.convertToString(stream)

            if (programConfig.useAPIAssets) {
                RemoteAPIFilesChecker.getLocalVersion(this)?.let { resVersionText = it }
            }

            val packInfo = packageManager.getPackageInfo(packageName, 0)
            val version = packInfo.versionName
            val versionCode = packInfo.longVersionCode
            versionText = "$version ($versionCode)"
        }
        catch (_: Exception) {}

        return listOf(versionText, resVersionText)
    }

    fun openUrl(url: String) {
        val webpage = Uri.parse(url)
        val intent = Intent(Intent.ACTION_VIEW, webpage)
        startActivity(intent)
    }

    override fun pushKeyEvent(event: KeyEvent): Boolean {
        return dispatchKeyEvent(event)
    }

    @SuppressLint("RestrictedApi")
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        // Log.d(TAG, "${event.keyCode}, ${event.action}")
        if (MainKeyEventDispatcher.checkDbgKey(event.keyCode, event.action)) {
            val origDbg = config.dbgMode
            config.dbgMode = !origDbg
            checkConfigAndUpdateView()
            saveConfig()
            showToast("TestMode: ${!origDbg}")
        }
        return if (event.action == 1145) true else super.dispatchKeyEvent(event)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        loadConfig()

        factory = UserConfigViewModelFactory(config)
        viewModel = ViewModelProvider(this, factory)[UserConfigViewModel::class.java]

        programConfigFactory = ProgramConfigViewModelFactory(programConfig,
            FileHotUpdater.getZipResourceVersion(File(filesDir, "update_trans.zip").absolutePath).toString()
        )
        programConfigViewModel = ViewModelProvider(this, programConfigFactory)[ProgramConfigViewModel::class.java]

        ShizukuApi.init()

        if (socketServer.startServer()) {
            Log.i(TAG, "Duplex socket server started in main Activity")
        } else {
            Log.e(TAG, "Failed to start duplex socket server in main Activity")
        }

        setContent {
            LocalifyTheme(dynamicColor = false, darkTheme = false) {
                MainUI(context = this)
            }
        }
    }
}


@Composable
fun getConfigState(context: MainActivity?, previewData: LinkuraConfig?): State<LinkuraConfig> {
    return if (context != null) {
        context.viewModel.config.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow(previewData!!)
        configMSF.asStateFlow().collectAsState()
    }
}

@Composable
fun getProgramConfigState(context: MainActivity?, previewData: ProgramConfig? = null): State<ProgramConfig> {
    return if (context != null) {
        context.programConfigViewModel.config.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow(previewData ?: ProgramConfig())
        configMSF.asStateFlow().collectAsState()
    }
}

@Composable
fun getProgramDownloadState(context: MainActivity?): State<Float> {
    return if (context != null) {
        context.programConfigViewModel.downloadProgress.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow(0f)
        configMSF.asStateFlow().collectAsState()
    }
}

@Composable
fun getProgramDownloadAbleState(context: MainActivity?): State<Boolean> {
    return if (context != null) {
        context.programConfigViewModel.downloadAble.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow(true)
        configMSF.asStateFlow().collectAsState()
    }
}

@Composable
fun getProgramLocalResourceVersionState(context: MainActivity?): State<String> {
    return if (context != null) {
        context.programConfigViewModel.localResourceVersion.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow("null")
        configMSF.asStateFlow().collectAsState()
    }
}

@Composable
fun getProgramLocalAPIResourceVersionState(context: MainActivity?): State<String> {
    return if (context != null) {
        context.programConfigViewModel.localAPIResourceVersion.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow("null")
        configMSF.asStateFlow().collectAsState()
    }
}

@Composable
fun getProgramDownloadErrorStringState(context: MainActivity?): State<String> {
    return if (context != null) {
        context.programConfigViewModel.errorString.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow("")
        configMSF.asStateFlow().collectAsState()
    }
}

@Composable
fun getMainUIConfirmState(context: MainActivity?, previewData: ConfirmStateModel? = null): State<ConfirmStateModel> {
    return if (context != null) {
        context.programConfigViewModel.mainUIConfirm.collectAsState()
    }
    else {
        val configMSF = MutableStateFlow(previewData ?: ConfirmStateModel())
        configMSF.asStateFlow().collectAsState()
    }
}
