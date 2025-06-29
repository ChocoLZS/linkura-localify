package io.github.chocolzs.linkura.localify.mainUtils


import net.lingala.zip4j.ZipFile
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream


object LSPatchUtils {
    fun deleteDirectory(directoryToBeDeleted: File): Boolean {
        if (directoryToBeDeleted.isDirectory) {
            val children = directoryToBeDeleted.listFiles()
            if (children != null) {
                for (child in children) {
                    deleteDirectory(child)
                }
            }
        }
        return directoryToBeDeleted.delete()
    }

    fun unzipXAPK(inputStream: InputStream, destDir: String) {
        val destDirFile = File(destDir)
        if (!destDirFile.exists()) {
            destDirFile.mkdirs()
        }

        val tempFile = File.createTempFile("xapk_temp", ".zip", destDirFile)
        tempFile.deleteOnExit()

        inputStream.use { input ->
            FileOutputStream(tempFile).use { output ->
                input.copyTo(output)
            }
        }

        ZipFile(tempFile).extractAll(destDir)
        tempFile.delete()
    }

    fun unzipXAPKWithProgress(inputStream: InputStream, destDir: String, progressCallback: (Int) -> Unit) {
        val destDirFile = File(destDir)
        if (!destDirFile.exists()) {
            destDirFile.mkdirs()
        }

        val tempFile = File.createTempFile("xapk_temp", ".zip", destDirFile)
        tempFile.deleteOnExit()

        inputStream.use { input ->
            FileOutputStream(tempFile).use { output ->
                input.copyTo(output)
            }
        }

        val zipFile = ZipFile(tempFile)
        val progressMonitor = zipFile.progressMonitor
        val extractionThread = Thread {
            zipFile.extractAll(destDir)
        }

        extractionThread.start()

        while (extractionThread.isAlive) {
            progressCallback(progressMonitor.percentDone)
            Thread.sleep(100)
        }

        progressCallback(100)
        tempFile.delete()
    }


}