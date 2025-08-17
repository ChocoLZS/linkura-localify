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
import io.github.chocolzs.linkura.localify.ipc.LinkuraAidlService
import io.github.chocolzs.linkura.localify.mainUtils.ArchiveRepository
import io.github.chocolzs.linkura.localify.mainUtils.LogExporter
import io.github.chocolzs.linkura.localify.mainUtils.RemoteAPIFilesChecker
import io.github.chocolzs.linkura.localify.mainUtils.ShizukuApi
import io.github.chocolzs.linkura.localify.mainUtils.json
import io.github.chocolzs.linkura.localify.models.ConfirmStateModel
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.ProgramConfig
import io.github.chocolzs.linkura.localify.models.ProgramConfigViewModel
import io.github.chocolzs.linkura.localify.models.ProgramConfigViewModelFactory
import io.github.chocolzs.linkura.localify.ui.pages.MainUI
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme
import io.github.chocolzs.linkura.localify.utils.CameraSensitivityState
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.serialization.encodeToString
import java.io.File


class MainActivity : ComponentActivity(), ConfigUpdateListener, IConfigurableActivity<MainActivity> {
    override lateinit var config: LinkuraConfig
    override lateinit var programConfig: ProgramConfig

    override lateinit var factory: UserConfigViewModelFactory
    override lateinit var viewModel: UserConfigViewModel

    override lateinit var programConfigFactory: ProgramConfigViewModelFactory
    override lateinit var programConfigViewModel: ProgramConfigViewModel

    private val aidlServiceIntent: Intent by lazy { 
        Intent(this, LinkuraAidlService::class.java)
    }


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

    fun exportLogs() {
        LogExporter.addLogEntry("MainActivity", "I", "Export logs requested by user")
        val logFile = LogExporter.exportLogs(this)
        if (logFile != null) {
            showToast(getString(R.string.log_export_success))
            LogExporter.addLogEntry("MainActivity", "I", "Log export successful: ${logFile.name}")
        } else {
            showToast(getString(R.string.log_export_failed))
            LogExporter.addLogEntry("MainActivity", "E", "Log export failed")
        }
    }

    private fun performFirstLaunchArchiveRefresh() {
        val prefs = getSharedPreferences("linkura_prefs", 0)
        val isFirstLaunch = prefs.getBoolean("is_first_launch", true)
        val lastRefreshTime = prefs.getLong("last_archive_refresh", 0)
        val currentTime = System.currentTimeMillis()
        val oneHourInMs = 60 * 60 * 1000L

        if (isFirstLaunch || (currentTime - lastRefreshTime) > oneHourInMs) {
            LogExporter.addLogEntry("MainActivity", "I", "Performing automatic archive refresh on first launch")
            
            CoroutineScope(Dispatchers.IO).launch {
                try {
                    val defaultMetadataUrl = getString(R.string.replay_default_metadata_url)
                    val savedMetadataUrl = prefs.getString("metadata_url", defaultMetadataUrl) ?: defaultMetadataUrl
                    val result = ArchiveRepository.fetchArchiveList(savedMetadataUrl)
                    
                    result.onSuccess { fetchedList ->
                        // Save archive list
                        ArchiveRepository.saveArchiveList(this@MainActivity, fetchedList)
                        
                        // Update archive config
                        val existingConfig = ArchiveRepository.loadArchiveConfig(this@MainActivity)
                        val newConfig = ArchiveRepository.createArchiveConfigFromList(fetchedList, existingConfig)
                        ArchiveRepository.saveArchiveConfig(this@MainActivity, newConfig)
                        
                        LogExporter.addLogEntry("MainActivity", "I", "Archive refresh successful: ${fetchedList.size} items")
                    }.onFailure { error ->
                        LogExporter.addLogEntry("MainActivity", "E", "Archive refresh failed: ${error.message}")
                    }
                } catch (e: Exception) {
                    LogExporter.addLogEntry("MainActivity", "E", "Archive refresh error: ${e.message}")
                }
                
                // Update preferences
                prefs.edit()
                    .putBoolean("is_first_launch", false)
                    .putLong("last_archive_refresh", currentTime)
                    .apply()
            }
        }
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

        LogExporter.addLogEntry("MainActivity", "I", "MainActivity onCreate called")
        loadConfig()

        factory = UserConfigViewModelFactory(config)
        viewModel = ViewModelProvider(this, factory)[UserConfigViewModel::class.java]

        programConfigFactory = ProgramConfigViewModelFactory(programConfig,
            FileHotUpdater.getZipResourceVersion(File(filesDir, "update_trans.zip").absolutePath).toString()
        )
        programConfigViewModel = ViewModelProvider(this, programConfigFactory)[ProgramConfigViewModel::class.java]

        ShizukuApi.init()
        
        // Initialize global camera sensitivity state
        CameraSensitivityState.initialize(this)

        // Auto-refresh archive data on first launch
        performFirstLaunchArchiveRefresh()

        // Start AIDL service
        try {
            startService(aidlServiceIntent)
            Log.i(TAG, "AIDL service started in main Activity")
            LogExporter.addLogEntry(TAG, "I", "AIDL service started from MainActivity")
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start AIDL service in main Activity", e)
            LogExporter.addLogEntry(TAG, "E", "Failed to start AIDL service: ${e.message}")
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
