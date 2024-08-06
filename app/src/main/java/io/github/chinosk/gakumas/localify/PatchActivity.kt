package io.github.chinosk.gakumas.localify

import android.content.Context
import android.content.pm.PackageInstaller
import android.net.Uri
import android.os.Bundle
import android.os.Environment
import android.provider.OpenableColumns
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.core.content.FileProvider
import io.github.chinosk.gakumas.localify.mainUtils.IOnShell
import io.github.chinosk.gakumas.localify.mainUtils.LSPatchUtils
import io.github.chinosk.gakumas.localify.mainUtils.ShizukuApi
import io.github.chinosk.gakumas.localify.mainUtils.ShizukuShell
import io.github.chinosk.gakumas.localify.ui.components.InstallDiag
import io.github.chinosk.gakumas.localify.ui.pages.PatchPage
import io.github.chinosk.gakumas.localify.ui.theme.GakumasLocalifyTheme
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.lsposed.patch.LSPatch
import org.lsposed.patch.util.Logger
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.io.OutputStream
import java.nio.file.Files
import java.nio.file.attribute.PosixFilePermissions


interface PatchCallback {
    fun onLog(message: String, isError: Boolean = false)
    fun onSuccess(outFiles: List<File>)
    fun onFailed(msg: String, exception: Throwable? = null)
}

val patchTag = "${TAG}-Patcher"


open class PatchLogger : Logger() {
    override fun d(msg: String) {
        if (this.verbose) {
            Log.d(patchTag, msg)
        }
    }

    override fun i(msg: String) {
        Log.i(patchTag, msg)
    }

    override fun e(msg: String) {
        Log.e(patchTag, msg)
    }
}


class LSPatchExt(outputDir: String, isDebuggable: Boolean, localMode: Boolean, logger: Logger) : LSPatch(logger, "123.apk --debuggable --manager -l 2") {
    init {
        val parentClass = LSPatch::class.java
        // val apkPathsField = parentClass.getDeclaredField("apkPaths")
        val outputPathField = parentClass.getDeclaredField("outputPath")
        val forceOverwriteField = parentClass.getDeclaredField("forceOverwrite")
        val debuggableFlagField = parentClass.getDeclaredField("debuggableFlag")
        val useManagerField = parentClass.getDeclaredField("useManager")

        // apkPathsField.isAccessible = true
        outputPathField.isAccessible = true
        forceOverwriteField.isAccessible = true
        debuggableFlagField.isAccessible = true
        useManagerField.isAccessible = true

        // apkPathsField.set(this, apkPaths)
        forceOverwriteField.set(this, true)
        outputPathField.set(this, outputDir)
        debuggableFlagField.set(this, isDebuggable)
        useManagerField.set(this, localMode)
    }

    fun setModules(modules: List<String>) {
        val parentClass = LSPatch::class.java
        val modulesField = parentClass.getDeclaredField("modules")
        modulesField.isAccessible = true
        modulesField.set(this, modules)
    }
}


class PatchActivity : ComponentActivity() {
    private lateinit var outputDir: String
    private var mOutFiles: List<File> = listOf()
    private var reservePatchFiles: Boolean = false
    var patchCallback: PatchCallback? = null

    private fun handleSelectedFile(uri: Uri) {
        val fileName = uri.path?.substringAfterLast('/')
        if (fileName != null) {
            Log.d(patchTag, fileName)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        outputDir = "${filesDir.absolutePath}/output"
        // ShizukuApi.init()

        setContent {
            GakumasLocalifyTheme(dynamicColor = false, darkTheme = false) {
                val scope = rememberCoroutineScope()
                var installing by remember { mutableStateOf(false) }

                PatchPage() { apks, isPatchLocalMode, isPatchDebuggable, isReservePatchFiles, onFinish, onLog ->
                    reservePatchFiles = isReservePatchFiles

                    onClickPatch(apks, isPatchLocalMode, isPatchDebuggable, object : PatchCallback {
                        init {
                            patchCallback = this
                        }

                        override fun onLog(message: String, isError: Boolean) {
                            onLog(message, isError)
                        }

                        override fun onSuccess(outFiles: List<File>) {
                            // Handle success, e.g., notify user or update UI
                            Log.i(patchTag, "Patch succeeded: $outFiles")
                            onLog("Patch succeeded: $outFiles")
                            onFinish()

                            scope.launch {
                                mOutFiles = outFiles
                                installing = true
                            }
                        }

                        override fun onFailed(msg: String, exception: Throwable?) {
                            Log.i(patchTag, "Patch failed: $msg", exception)
                            onLog("Patch failed: $msg\n$exception", true)
                            LSPatchUtils.deleteDirectory(File(outputDir))
                            onFinish()
                        }
                    })
                }

                if (installing) InstallDiag(this@PatchActivity, mOutFiles, patchCallback, reservePatchFiles) { _, _ ->
                    installing = false
                    mOutFiles = listOf()
                }

            }
        }
    }

    private fun onClickPatch(apkPaths: List<Uri>, isLocalMode: Boolean, isDebuggable: Boolean, callback: PatchCallback) {
        var isPureApk = true

        for (i in apkPaths) {  // 判断是否全是apk
            val fileName = getFileName(i)
            if (fileName == null) {
                callback.onFailed("Get file name failed: $i")
                return
            }
            else {
                if (!fileName.lowercase().endsWith(".apk")) {
                    isPureApk = false
                }
            }
        }

        if (apkPaths.size != 1 && !isPureApk) {  // 多选，非全 apk
            callback.onFailed("Multiple selection files must be all apk files.")
            return
        }

        if (isPureApk) {
            val apks: MutableList<File> = mutableListOf()
            // val apkPathStr: MutableList<String> = mutableListOf()
            for (i in apkPaths) {
                val apkFile = uriToFile(i)
                if (apkFile == null) {
                    callback.onFailed("Get file failed: $i")
                    return
                }
                apks.add(apkFile)
                // apkPathStr.add(apkFile.absolutePath)
            }
            patchApks(apks, isLocalMode, isDebuggable, callback)
            return
        }

        val fileUri = apkPaths[0]
        val fileName = getFileName(fileUri)
        if (fileName == null) {
            callback.onFailed("Get file name failed: $fileUri")
            return
        }
        val lowerName = fileName.lowercase()
        if (!(lowerName.endsWith("apks") || lowerName.endsWith("xapk") || lowerName.endsWith("zip"))) {
            callback.onFailed("Unknown file: $fileName")
            return
        }

        val inputStream: InputStream? = contentResolver.openInputStream(fileUri)
        if (inputStream == null) {
            callback.onFailed("Open file failed: $fileUri")
            return
        }
        val unzipCacheDir = File(cacheDir, "apks_unzip")
        if (unzipCacheDir.exists()) {
            LSPatchUtils.deleteDirectory(unzipCacheDir)
        }
        unzipCacheDir.mkdirs()

        CoroutineScope(Dispatchers.IO).launch {
            // FileHotUpdater.unzip(inputStream, unzipCacheDir.absolutePath)
            withContext(Dispatchers.Main) {
                callback.onLog("Unzipping...")
            }

            LSPatchUtils.unzipXAPKWithProgress(inputStream, unzipCacheDir.absolutePath) { /*percent ->
                runOnUiThread {
                    Log.d(TAG, "unzip: $percent")
                }*/
            }

            val files = unzipCacheDir.listFiles()
            if (files == null) {
                withContext(Dispatchers.Main) {
                    callback.onFailed("Can't get unzip files: $fileName")
                }
                return@launch
            }

            withContext(Dispatchers.Main) {
                callback.onLog("Unzip completed.")
            }

            val apks: MutableList<File> = mutableListOf()
            for (file in files) {
                if (file.isFile) {
                    if (file.name.lowercase().endsWith(".apk")) {
                        apks.add(file)
                    }
                }
            }
            patchApks(apks, isLocalMode, isDebuggable, callback) {
                LSPatchUtils.deleteDirectory(unzipCacheDir)
            }
        }
    }

    private fun patchApks(apks: List<File>, isLocalMode: Boolean, isDebuggable: Boolean,
                          callback: PatchCallback, onPatchEnd: (() -> Unit)? = null) {

        CoroutineScope(Dispatchers.IO).launch {
            try {
                val lspatch = LSPatchExt(outputDir, isDebuggable, isLocalMode, object : PatchLogger() {
                    override fun d(msg: String) {
                        super.d(msg)
                        runOnUiThread {
                            callback.onLog(msg)
                        }
                    }

                    override fun i(msg: String) {
                        super.i(msg)
                        runOnUiThread {
                            callback.onLog(msg)
                        }
                    }

                    override fun e(msg: String) {
                        super.e(msg)
                        runOnUiThread {
                            callback.onLog(msg, true)
                        }
                    }
                })

                if (!isLocalMode) {
                    lspatch.setModules(listOf(applicationInfo.sourceDir))
                }

                withContext(Dispatchers.Main) {
                    callback.onLog("Patching started.")
                }

                // lspatch.doCommandLine()
                val outBasePath = File(filesDir, "output")
                if (!outBasePath.exists()) {
                    outBasePath.mkdirs()
                }

                val outFiles: MutableList<File> = mutableListOf()
                for (i in apks) {
                    val outFile = File(outBasePath, "patch-${i.name}")
                    if (outFile.exists()) {
                        outFile.delete()
                    }
                    callback.onLog("Patching $i")
                    lspatch.patch(i, outFile)
                    i.delete()
                    outFiles.add(outFile)
                }

                withContext(Dispatchers.Main) {
                    callback.onLog("Patching completed.")
                    callback.onSuccess(outFiles)
                }
            } catch (e: Error) {
                // Log error and call the failure callback
                Log.e(patchTag, "Patch error", e)
                withContext(Dispatchers.Main) {
                    callback.onFailed("Patch error: ${e.message}", e)
                }
            } catch (e: Exception) {
                // Log exception and call the failure callback
                Log.e(patchTag, "Patch exception", e)
                withContext(Dispatchers.Main) {
                    callback.onFailed("Patch exception: ${e.message}", e)
                }
            }
            finally {
                onPatchEnd?.let { it() }
            }
        }
    }

    private fun getFileName(uri: Uri): String? {
        var fileName: String? = null
        val cursor = contentResolver.query(uri, null, null, null, null)
        cursor?.use {
            if (it.moveToFirst()) {
                fileName = it.getString(it.getColumnIndexOrThrow(OpenableColumns.DISPLAY_NAME))
            }
        }
        return fileName
    }

    private fun uriToFile(uri: Uri): File? {
        val fileName = getFileName(uri) ?: return null
        val file = File(cacheDir, fileName)
        try {
            val inputStream: InputStream? = contentResolver.openInputStream(uri)
            val outputStream: OutputStream = FileOutputStream(file)
            inputStream?.use { input ->
                outputStream.use { output ->
                    val buffer = ByteArray(4 * 1024) // 4KB
                    var read: Int
                    while (input.read(buffer).also { read = it } != -1) {
                        output.write(buffer, 0, read)
                    }
                    output.flush()
                }
            }
            return file
        } catch (e: Exception) {
            e.printStackTrace()
        }
        return null
    }

    companion object {

        fun getUriFromFile(context: Context, file: File): Uri {
            return FileProvider.getUriForFile(
                context,
                "io.github.chinosk.gakumas.localify.fileprovider",
                file
            )
        }

        fun saveFileTo(apkFiles: List<File>, targetDirectory: File, isMove: Boolean,
                       enablePermission: Boolean): List<File> {
            val hasDirectory = if (!targetDirectory.exists()) {
                targetDirectory.mkdirs()
            } else {
                true
            }
            if (!hasDirectory) {
                throw NoSuchFileException(targetDirectory, reason = "check targetDirectory failed.")
            }

            if (enablePermission) {
                try {
                    val origPermission = Files.getPosixFilePermissions(targetDirectory.toPath())
                    val requiredPermissions = PosixFilePermissions.fromString("rwxrwxrwx")
                    if (!origPermission.equals(requiredPermissions)) {
                        Files.setPosixFilePermissions(targetDirectory.toPath(), requiredPermissions)
                    }
                }
                catch (e: Exception) {
                    Log.e(TAG, "checkPosixFilePermissions failed.", e)
                }
            }

            val movedFiles: MutableList<File> = mutableListOf()
            apkFiles.forEach { file ->
                val targetFile = File(targetDirectory, file.name)
                if (targetFile.exists()) targetFile.delete()
                file.copyTo(targetFile)
                movedFiles.add(targetFile)
                if (isMove) {
                    file.delete()
                }
            }
            return movedFiles
        }

        suspend fun installSplitApks(context: Context, apkFiles: List<File>, reservePatchFiles: Boolean,
                                     patchCallback: PatchCallback?): Pair<Int, String?> {
            Log.i(TAG, "Perform install patched apks")
            var status = PackageInstaller.STATUS_FAILURE
            var message: String? = null

            withContext(Dispatchers.IO) {
                runCatching {
                    val sdcardPath = Environment.getExternalStorageDirectory().path
                    val targetDirectory = File(sdcardPath, "Download/gkms_local_patch")
                    val savedFiles = saveFileTo(apkFiles, targetDirectory, true, false)
                    patchCallback?.onLog("Patched files: $savedFiles")

                    if (!ShizukuApi.isPermissionGranted) {
                        status = PackageInstaller.STATUS_FAILURE
                        message = "Shizuku Not Ready."
                        if (!reservePatchFiles) savedFiles.forEach { file -> if (file.exists()) file.delete() }
                        return@runCatching
                    }

                    val ioShell = object: IOnShell {
                        override fun onShellLine(msg: String) {
                            patchCallback?.onLog(msg)
                        }

                        override fun onShellError(msg: String) {
                            patchCallback?.onLog(msg, true)
                        }
                    }

                    if (ShizukuApi.isPackageInstalledWithoutPatch("com.bandainamcoent.idolmaster_gakuen")) {
                        val uninstallShell = ShizukuShell(mutableListOf(), "pm uninstall com.bandainamcoent.idolmaster_gakuen", ioShell)
                        uninstallShell.exec()
                        uninstallShell.destroy()
                    }

                    val installDS = "/data/local/tmp/gkms_local_patch"

                    val action = if (reservePatchFiles) "cp" else "mv"
                    val copyFilesCmd: MutableList<String> = mutableListOf()
                    val movedFiles: MutableList<String> = mutableListOf()
                    savedFiles.forEach { file ->
                        val movedFileName = "$installDS/${file.name}"
                        movedFiles.add(movedFileName)
                        copyFilesCmd.add("$action ${file.absolutePath} $movedFileName")
                    }
                    val moveFileCommand = "mkdir $installDS && " +
                            "chmod 777 $installDS && " +
                            copyFilesCmd.joinToString(" && ")
                    Log.d(TAG, "moveFileCommand: $moveFileCommand")

                    val cpFileShell = ShizukuShell(mutableListOf(), moveFileCommand, ioShell)
                    cpFileShell.exec()
                    cpFileShell.destroy()

                    val installFiles = movedFiles.joinToString(" ")
                    val command = "pm install -r $installFiles && rm $installFiles"
                    Log.d(TAG, "shell: $command")
                    val sh = ShizukuShell(mutableListOf(), command, ioShell)
                    sh.exec()
                    sh.destroy()

                    status = PackageInstaller.STATUS_SUCCESS
                    message = "Done."
                }.onFailure { e ->
                    status = PackageInstaller.STATUS_FAILURE
                    message = e.stackTraceToString()
                }
            }
            return Pair(status, message)
        }
    }
}
