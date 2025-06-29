package io.github.chocolzs.linkura.localify.ui.components

import android.content.Context
import android.content.pm.PackageInstaller
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import android.util.Log
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import io.github.chocolzs.linkura.localify.PatchActivity
import io.github.chocolzs.linkura.localify.PatchCallback
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.TAG
import java.io.File


@Composable
fun InstallDiag(context: PatchActivity?, apkFiles: List<File>, patchCallback: PatchCallback?, reservePatchFiles: Boolean,
                onFinish: (Int, String?) -> Unit) {
    // val scope = rememberCoroutineScope()
    // var uninstallFirst by remember { mutableStateOf(ShizukuApi.isPackageInstalledWithoutPatch(patchApp.app.packageName)) }
    var installing by remember { mutableStateOf(-1) }
    var showInstallConfirm by remember { mutableStateOf(true) }

    fun finish(code: Int, msg: String?) {
        patchCallback?.onLog("Install finished($code): $msg")
        onFinish(code, msg)
        if (code != PackageInstaller.STATUS_SUCCESS) {
            msg?.let{ patchCallback?.onFailed(it) }
        }
    }

    suspend fun doInstall() {
        Log.i(TAG, "Installing app $apkFiles")
        installing = 1
        val (status, message) = PatchActivity.installSplitApks(context!!, apkFiles, reservePatchFiles, patchCallback)
        installing = 0
        Log.i(TAG, "Installation end: $status, $message")
        finish(status, message)
    }

    LaunchedEffect(showInstallConfirm) {
        if (installing == 0) {
            doInstall()
        }
    }

    if (showInstallConfirm) {
        Box {
            GakuGroupConfirm(
                title = stringResource(R.string.install),
                // initIsVisible = true,
                onCancel = {
                    showInstallConfirm = false
                    installing = -1
                    finish(PackageInstaller.STATUS_FAILURE, "User Cancelled.")
                    showInstallConfirm = true },
                onConfirm = {
                    showInstallConfirm = false
                    installing = 0 },
                contentHeightForAnimation = 500f
            ) {
                Column {
                    Text(stringResource(R.string.patch_finished))
                }
            }
        }
        return
    }

    if (installing == 1) {
        AlertDialog(
            onDismissRequest = {},
            confirmButton = {},
            title = {
                Text(
                    modifier = Modifier.fillMaxWidth(),
                    text = stringResource(R.string.installing),
                    textAlign = TextAlign.Center
                )
            }
        )
    }
}

@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO, widthDp = 380)
@Composable
fun InstallDiagPreview(modifier: Modifier = Modifier) {
    InstallDiag(null, listOf(), null, false) { _, _ -> }
}

