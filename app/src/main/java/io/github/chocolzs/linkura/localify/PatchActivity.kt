package io.github.chocolzs.linkura.localify

import android.Manifest
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import android.content.pm.PackageManager
import android.media.MediaScannerConnection
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.MediaStore
import android.provider.OpenableColumns
import android.util.Log
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import androidx.core.content.FileProvider
import io.github.chocolzs.linkura.localify.mainUtils.IOnShell
import io.github.chocolzs.linkura.localify.mainUtils.LSPatchUtils
import io.github.chocolzs.linkura.localify.mainUtils.ShizukuApi
import io.github.chocolzs.linkura.localify.mainUtils.ShizukuShell
import io.github.chocolzs.linkura.localify.ui.components.InstallDiag
import io.github.chocolzs.linkura.localify.ui.pages.PatchPage
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.lsposed.patch.LSPatch
import org.lsposed.patch.util.Logger
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.InputStream
import java.io.OutputStream
import java.nio.file.Files
import java.nio.file.attribute.PosixFilePermissions
import java.util.concurrent.CountDownLatch


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

    fun openUrl(url: String) {
        val webpage = Uri.parse(url)
        val intent = Intent(Intent.ACTION_VIEW, webpage)
        startActivity(intent)
    }

    private val writePermissionLauncher = registerForActivityResult(
        ActivityResultContracts.StartIntentSenderForResult()
    ) { result ->
        if (result.resultCode != RESULT_OK) {
            Toast.makeText(this, "Permission Request Failed.", Toast.LENGTH_SHORT).show()
            finish()
        }
    }

    private val writePermissionLauncherQ = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        if (!isGranted) {
            Toast.makeText(this, "Permission Request Failed.", Toast.LENGTH_SHORT).show()
            finish()
        }
    }

    private fun checkAndRequestWritePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            /*
            // 针对 API 级别 30 及以上使用 MediaStore.createWriteRequest
            val uri = MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
            val intentSender = MediaStore.createWriteRequest(contentResolver, listOf(uri)).intentSender
            writePermissionLauncher.launch(IntentSenderRequest.Builder(intentSender).build())*/
        }
        else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.Q) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
                // 请求 WRITE_EXTERNAL_STORAGE 权限
                writePermissionLauncherQ.launch(Manifest.permission.WRITE_EXTERNAL_STORAGE)
            }
        }
    }


    private fun writeFileToDownloadFolder(
        sourceFile: File,
        targetFolder: String,
        targetFileName: String
    ): Boolean {
        val downloadDirectory = Environment.DIRECTORY_DOWNLOADS
        val relativePath = "$downloadDirectory/$targetFolder/"
        val resolver = contentResolver

        // 检查文件是否已经存在
        val existingUri = MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
        val query = resolver.query(
            existingUri,
            arrayOf(MediaStore.Files.FileColumns._ID),
            "${MediaStore.Files.FileColumns.RELATIVE_PATH}=? AND ${MediaStore.Files.FileColumns.DISPLAY_NAME}=?",
            arrayOf(relativePath, targetFileName),
            null
        )

        query?.use {
            if (it.moveToFirst()) {
                // 如果文件存在，则删除
                val id = it.getLong(it.getColumnIndexOrThrow(MediaStore.Files.FileColumns._ID))
                val deleteUri = MediaStore.Files.getContentUri("external", id)
                resolver.delete(deleteUri, null, null)
                Log.d(patchTag, "query delete: $deleteUri")
            }
        }

        val contentValues = ContentValues().apply {
            put(MediaStore.Downloads.DISPLAY_NAME, targetFileName)
            put(MediaStore.Downloads.MIME_TYPE, "application/octet-stream")
            put(MediaStore.Downloads.RELATIVE_PATH, relativePath)
        }

        var uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, contentValues)
        Log.d(patchTag, "insert uri: $uri")

        if (uri == null) {
            val latch = CountDownLatch(1)
            val downloadDirectory = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
            val downloadSaveDirectory = File(downloadDirectory, targetFolder)
            val downloadSaveFile = File(downloadSaveDirectory, targetFileName)
            MediaScannerConnection.scanFile(this, arrayOf(downloadSaveFile.absolutePath),
                null
            ) { _, _ ->
                Log.d(patchTag, "scanFile finished.")
                latch.countDown()
            }
            latch.await()
            uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, contentValues)
            if (uri == null) {
                Log.e(patchTag, "uri is still null")
                return false
            }
        }

        return try {
            resolver.openOutputStream(uri)?.use { outputStream ->
                FileInputStream(sourceFile).use { inputStream ->
                    inputStream.copyTo(outputStream)
                }
            }
            contentValues.clear()
            contentValues.put(MediaStore.Downloads.IS_PENDING, 0)
            resolver.update(uri, contentValues, null, null)
            true
        } catch (e: Exception) {
            resolver.delete(uri, null, null)
            e.printStackTrace()
            false
        }
    }


    private fun deleteFileInDownloadFolder(targetFolder: String, targetFileName: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val selection =
                "${MediaStore.MediaColumns.RELATIVE_PATH} = ? AND ${MediaStore.MediaColumns.DISPLAY_NAME} = ?"
            val selectionArgs =
                arrayOf("${Environment.DIRECTORY_DOWNLOADS}/$targetFolder/", targetFileName)

            val uri = MediaStore.Files.getContentUri(MediaStore.VOLUME_EXTERNAL_PRIMARY)
            contentResolver.delete(uri, selection, selectionArgs)
        }
        else {
            val file = File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "$targetFolder/$targetFileName")
            if (file.exists()) {
                if (file.delete()) {
                    // Toast.makeText(this, "文件已删除", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }

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
        checkAndRequestWritePermission()

        setContent {
            LocalifyTheme(dynamicColor = false, darkTheme = false) {
                val scope = rememberCoroutineScope()
                var installing by remember { mutableStateOf(false) }

                PatchPage(
                    onOpenUrl = { url -> openUrl(url) }
                ) { apks, isPatchLocalMode, isPatchDebuggable, isReservePatchFiles, onFinish, onLog ->
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
                "io.github.chocolzs.linkura.localify.fileprovider",
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

        private fun generateNonce(size: Int): String {
            val nonceScope = "1234567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
            val scopeSize = nonceScope.length
            val nonceItem: (Int) -> Char = { nonceScope[(scopeSize * Math.random()).toInt()] }
            return Array(size, nonceItem).joinToString("")
        }

        fun saveFilesToDownload(context: PatchActivity, apkFiles: List<File>, targetFolder: String,
                                isMove: Boolean): List<String>? {
            val ret: MutableList<String> = mutableListOf()
            apkFiles.forEach { f ->
                val success = context.writeFileToDownloadFolder(f, targetFolder, f.name)
                if (success) {
                    ret.add(f.name)
                }
                else {
                    val newName = "${generateNonce(6)}${f.name}"
                    val success2 = context.writeFileToDownloadFolder(f, targetFolder,
                        newName)
                    if (!success2) {
                        return null
                    }
                    ret.add(newName)
                }
                if (isMove) {
                    f.delete()
                }
            }
            return ret
        }

        suspend fun installSplitApks(context: PatchActivity, apkFiles: List<File>, reservePatchFiles: Boolean,
                                     patchCallback: PatchCallback?): Pair<Int, String?> {
            Log.i(TAG, "Perform install patched apks")
            var status = PackageInstaller.STATUS_FAILURE
            var message: String? = null

            withContext(Dispatchers.IO) {
                runCatching {
                    val sdcardPath = Environment.getExternalStorageDirectory().path
                    val targetDirectory = File(sdcardPath, "Download/linkura_local_patch")
                    // val savedFiles = saveFileTo(apkFiles, targetDirectory, true, false)

                    val savedFileNames = saveFilesToDownload(context, apkFiles, "linkura_local_patch", true)
                    if (savedFileNames == null) {
                        status = PackageInstaller.STATUS_FAILURE
                        message = "Save files failed."
                        return@runCatching
                    }

                    // patchCallback?.onLog("Patched files: $savedFiles")
                    patchCallback?.onLog("Patched files: $apkFiles")

                    if (!ShizukuApi.isPermissionGranted) {
                        status = PackageInstaller.STATUS_FAILURE
                        message = "Shizuku Not Ready."
                        // if (!reservePatchFiles) savedFiles.forEach { file -> if (file.exists()) file.delete() }
                        if (!reservePatchFiles) {
                            savedFileNames.forEach { f ->
                                context.deleteFileInDownloadFolder("linkura_local_patch", f)
                            }
                        }
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

                    if (ShizukuApi.isPackageInstalledWithoutPatch("com.oddno.lovelive")) {
                        val uninstallShell = ShizukuShell(mutableListOf(), "pm uninstall com.oddno.lovelive", ioShell)
                        uninstallShell.exec()
                        uninstallShell.destroy()
                    }

                    val installDS = "/data/local/tmp/linkura_local_patch"

                    val action = if (reservePatchFiles) "cp" else "mv"
                    val copyFilesCmd: MutableList<String> = mutableListOf()
                    val movedFiles: MutableList<String> = mutableListOf()
                    savedFileNames.forEach { file ->
                        val movedFileName = "\"$installDS/${file}\""
                        movedFiles.add(movedFileName)
                        val dlSaveFileName = File(targetDirectory, file)
                        copyFilesCmd.add("$action ${dlSaveFileName.absolutePath} $movedFileName")
                    }
                    /*
                    savedFiles.forEach { file ->
                        val movedFileName = "$installDS/${file.name}"
                        movedFiles.add(movedFileName)
                        copyFilesCmd.add("$action ${file.absolutePath} $movedFileName")
                    }
                    */
                    val createDirCommand = "mkdir $installDS"
                    val moveFileCommand = "chmod 777 $installDS && " +
                            copyFilesCmd.joinToString(" && ")
                    Log.d(TAG, "moveFileCommand: $moveFileCommand")

                    ShizukuShell(mutableListOf(), createDirCommand, ioShell).exec().destroy()

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
