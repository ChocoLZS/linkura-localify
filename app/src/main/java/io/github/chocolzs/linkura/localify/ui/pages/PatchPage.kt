package io.github.chocolzs.linkura.localify.ui.pages

import android.content.pm.PackageManager
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import android.net.Uri
import android.os.Build
import android.util.Log
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.animateContentSize
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Warning
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.FloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.Saver
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshots.SnapshotStateList
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.activity.ComponentActivity
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.mainUtils.ShizukuApi
import io.github.chocolzs.linkura.localify.ui.components.GakuButton
import io.github.chocolzs.linkura.localify.ui.components.GakuGroupBox
import io.github.chocolzs.linkura.localify.ui.components.GakuGroupConfirm
import io.github.chocolzs.linkura.localify.ui.components.GakuRadio
import io.github.chocolzs.linkura.localify.ui.components.GakuSwitch
import io.github.chocolzs.linkura.localify.ui.components.base.CollapsibleBox
import io.github.chocolzs.linkura.localify.ui.components.icons.AutoFixHigh
import org.lsposed.lspatch.share.LSPConfig
import rikka.shizuku.Shizuku


data class LogText(var msg: String, val isErr: Boolean)

private val shizukuListener: (Int, Int) -> Unit = { _, grantResult ->
    ShizukuApi.isPermissionGranted = grantResult == PackageManager.PERMISSION_GRANTED
}


val LogTextListSaver = Saver<SnapshotStateList<LogText>, List<String>>(
    save = { logTextList -> logTextList.map { "${it.msg},${it.isErr}" } },
    restore = { savedStrings ->
        val restoredList = mutableStateListOf<LogText>()
        savedStrings.forEach { savedString ->
            val parts = savedString.split(",")
            restoredList.add(LogText(parts[0], parts[1].toBoolean()))
        }
        restoredList
    }
)


@Composable
fun PatchPage(modifier: Modifier = Modifier,
              onOpenUrl: ((String) -> Unit)? = null,
              content: (@Composable () -> Unit)? = null,
              onClickPatch: (selectFiles: List<Uri>, isLocalMode: Boolean, isDebuggable: Boolean,
                             reservePatchFiles: Boolean,
                             onFinishCallback: () -> Unit,
                             onLogCallback: (msg: String, isErr: Boolean) -> Unit) -> Unit) {
    LaunchedEffect(Unit) {
        Shizuku.addRequestPermissionResultListener(shizukuListener)
    }
    DisposableEffect(Unit) {
        onDispose {
            Shizuku.removeRequestPermissionResultListener(shizukuListener)
        }
    }

    val imagePainter = painterResource(R.drawable.bg_pattern)

    var isPatchLocalMode by rememberSaveable { mutableStateOf(true) }
    var isPatchDebuggable by rememberSaveable { mutableStateOf(true) }
    var isPatching by rememberSaveable { mutableStateOf(false) }
    var reservePatchFiles by rememberSaveable { mutableStateOf(false) }
    var showUninstallConfirm by remember { mutableStateOf(false) }

    val logMsgList = rememberSaveable(saver = LogTextListSaver) { mutableStateListOf(LogText("Patcher Logs", false)) }

    fun addLogMsg(msg: String, isErr: Boolean) {
        val length = logMsgList.size
        if (length == 0) {
            logMsgList.add(LogText(msg, isErr))
        }
        else {
            val lastLog = logMsgList[length - 1]
            if (lastLog.isErr == isErr) {
                // lastLog.msg += "\n${msg}"
                logMsgList[length - 1] = LogText("${lastLog.msg}\n$msg", isErr)
            }
            else {
                logMsgList.add(LogText(msg, isErr))
            }
        }
    }

    val storageLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { apks ->
        Log.d(TAG, apks.toString())
        if (apks.isEmpty()) {
            return@rememberLauncherForActivityResult
        }
        isPatching = true
        logMsgList.clear()
        onClickPatch(apks, isPatchLocalMode, isPatchDebuggable, reservePatchFiles, { isPatching = false }) { msg, err ->
            addLogMsg(msg, err)
        }
    }

    content?.let { it() }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFFFDFDFD))
    ) {
        val screenH = imageRepeater(
            painter = imagePainter,
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.TopCenter)
        )

        LazyColumn(
            modifier = modifier
                .fillMaxWidth()
                .sizeIn(maxHeight = screenH)
                .padding(10.dp, 10.dp, 10.dp, 0.dp),
            verticalArrangement = Arrangement.Top,
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            item {
                Column(modifier = modifier.fillMaxWidth()
                    .padding(10.dp, 10.dp, 10.dp, 0.dp)) {
                    Text(text = "Linkura Localify Patcher", fontSize = 18.sp)
                    Text(text = "LSPatch version: ${LSPConfig.instance.VERSION_NAME} (${LSPConfig.instance.VERSION_CODE})", fontSize = 13.sp)
                    Text(text = "Framework version: ${LSPConfig.instance.CORE_VERSION_NAME} (${LSPConfig.instance.CORE_VERSION_CODE}), API ${LSPConfig.instance.API_CODE}", fontSize = 13.sp)
                }
                Spacer(Modifier.height(6.dp))
            }

            item {
                GakuGroupBox(modifier = modifier, "Shizuku", contentPadding = 0.dp) {
                ElevatedCard(
                    shape = RoundedCornerShape(
                        bottomStart = 16.dp,
                        bottomEnd = 8.dp
                    ),
                    colors = CardDefaults.elevatedCardColors(containerColor = run {
                        if (ShizukuApi.isPermissionGranted) MaterialTheme.colorScheme.background
                        else MaterialTheme.colorScheme.errorContainer
                    })
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable {
                                if (ShizukuApi.isBinderAvailable && !ShizukuApi.isPermissionGranted) {
                                    Shizuku.requestPermission(114514)
                                }
                            }
                            .padding(18.dp, 10.dp, 18.dp, 14.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        if (ShizukuApi.isPermissionGranted) {
                            Icon(Icons.Outlined.CheckCircle, stringResource(R.string.shizuku_available))
                            Column(Modifier.padding(start = 20.dp)) {
                                Text(
                                    text = stringResource(R.string.shizuku_available),
                                    style = MaterialTheme.typography.titleMedium
                                )
                                Spacer(Modifier.height(4.dp))
                                Text(
                                    text = "API " + Shizuku.getVersion(),
                                    style = MaterialTheme.typography.bodyMedium
                                )
                            }
                        } else {
                            Icon(Icons.Outlined.Warning, stringResource(R.string.shizuku_unavailable))
                            Column(Modifier.padding(start = 20.dp)) {
                                Text(
                                    text = stringResource(R.string.shizuku_unavailable),
                                    style = MaterialTheme.typography.titleMedium
                                )
                                Spacer(Modifier.height(4.dp))
                                Text(
                                    text = stringResource(R.string.home_shizuku_warning),
                                    style = MaterialTheme.typography.bodyMedium
                                )
                            }
                        }
                    }
                }
                }
                Spacer(Modifier.height(6.dp))
            }

            item {
                GakuGroupBox(modifier = modifier, stringResource(R.string.game_patch)) {

                    Column(modifier = Modifier,
                        verticalArrangement = Arrangement.spacedBy(6.dp)) {

                        Text(stringResource(R.string.patch_mode))
                        Row(modifier = modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                            val radioModifier = remember {
                                modifier
                                    .height(40.dp)
                                    .weight(1f)
                            }

                            GakuRadio(modifier = radioModifier,
                                text = stringResource(R.string.patch_local), selected = isPatchLocalMode,
                                onClick = { isPatchLocalMode = true })

                            GakuRadio(modifier = radioModifier,
                                text = stringResource(R.string.patch_integrated), selected = !isPatchLocalMode,
                                onClick = { isPatchLocalMode = false })

                        }

                        // Android 15+ Warning
                        if (Build.VERSION.SDK_INT >= 35) {
                            Spacer(Modifier.height(8.dp))
                            ElevatedCard(
                                colors = CardDefaults.elevatedCardColors(
                                    containerColor = MaterialTheme.colorScheme.errorContainer
                                ),
                                modifier = Modifier.fillMaxWidth()
                            ) {
                                Column(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .padding(16.dp),
                                    verticalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    Text(
                                        text = stringResource(R.string.patch_android15_warning),
                                        style = MaterialTheme.typography.bodyMedium,
                                        fontWeight = FontWeight.Bold,
                                        color = MaterialTheme.colorScheme.onErrorContainer
                                    )
                                    GakuButton(
                                        text = stringResource(R.string.patch_android15_button),
                                        modifier = Modifier.fillMaxWidth(),
                                        onClick = {
                                            onOpenUrl?.invoke("https://github.com/JingMatrix/LSPatch")
                                        }
                                    )
                                }
                            }
                        }

                        CollapsibleBox(modifier = modifier,
                            expandState = true,
                            collapsedHeight = 0.dp,
                            showExpand = false
                        ) {
                            Box(modifier = Modifier.fillMaxWidth()) {
                                Text(text =  stringResource(
                                    if (isPatchLocalMode)
                                        R.string.patch_local_desc
                                    else
                                        R.string.patch_integrated_desc
                                ), color = Color.Gray, fontSize = 12.sp
                                )
                            }
                        }

                        GakuSwitch(modifier, stringResource(R.string.patch_debuggable), checked = isPatchDebuggable) {
                            isPatchDebuggable = !isPatchDebuggable
                        }

                        GakuSwitch(modifier, stringResource(R.string.reserve_patched), checked = reservePatchFiles) {
                            reservePatchFiles = !reservePatchFiles
                        }

                        Text(stringResource(R.string.support_file_types))

                        CollapsibleBox(modifier = modifier,
                            expandState = true,
                            collapsedHeight = 0.dp,
                            showExpand = false
                        ) {
                            Box(modifier = Modifier) {
                                LazyColumn(modifier = Modifier
                                    .fillMaxWidth()
                                    .sizeIn(maxHeight = screenH),
                                    reverseLayout = true) {
                                    logMsgList.asReversed().forEach { logText ->
                                        item {
                                            Text(modifier = Modifier.animateContentSize(),
                                                text = logText.msg,
                                                color = if (logText.isErr) Color.Red else Color.Black,
                                                fontSize = 12.sp)

                                        }
                                    }
                                }
                            }
                        }

                        Spacer(Modifier.height(0.dp))
                    }
                }
                Spacer(Modifier.height(12.dp))
            }

            item {
                Spacer(modifier = modifier.height(120.dp))
            }
        }

        FloatingActionButton(
            onClick = { if (!isPatching) {
                if (ShizukuApi.isPermissionGranted &&
                    ShizukuApi.isPackageInstalledWithoutPatch("com.oddno.lovelive")) {
                    showUninstallConfirm = true
                }
                else {
                    storageLauncher.launch(arrayOf("*/*"))
                }
            } },
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .padding(16.dp),
            containerColor = if (isPatching) Color.Gray else MaterialTheme.colorScheme.primary,
            shape = CircleShape
        ) {
            Icon(modifier = Modifier.size(24.dp),
                imageVector = Icons.Outlined.AutoFixHigh,
                contentDescription = "GotoPatch")
        }

        if (showUninstallConfirm) {
            GakuGroupConfirm(
                title = stringResource(R.string.warning),
                onCancel = { showUninstallConfirm = false },
                onConfirm = {
                    showUninstallConfirm = false
                    storageLauncher.launch(arrayOf("*/*"))},
                contentHeightForAnimation = screenH.value
            ) {
                Column {
                    Text(stringResource(R.string.patch_uninstall_text))
                    Text(stringResource(R.string.patch_uninstall_confirm))
                }
            }
        }

    }
}


@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO, widthDp = 680)
@Composable
fun PatchPagePreview(modifier: Modifier = Modifier) {
    PatchPage(modifier) { _, _, _, _, _, _ -> }
}
