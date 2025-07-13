package io.github.chocolzs.linkura.localify.mainUtils

import android.util.Log
import io.github.chocolzs.linkura.localify.TAG
import rikka.shizuku.Shizuku
import rikka.shizuku.ShizukuRemoteProcess
import java.io.BufferedReader
import java.io.InputStreamReader


val shellTag = "${TAG}_Shell"

interface IOnShell {
    fun onShellLine(msg: String)
    fun onShellError(msg: String)
}

/*
* Created by sunilpaulmathew <sunil.kde@gmail.com> on November 12, 2022
*/
class ShizukuShell(private var mOutput: MutableList<String>, private var mCommand: String,
    private val shellCallback: IOnShell? = null) {
    val isBusy: Boolean
        get() = mOutput.size > 0 && mOutput[mOutput.size - 1] != "aShell: Finish"

    fun exec(): ShizukuShell {
        try {
            Log.i(shellTag, "Execute: $mCommand")
            shellCallback?.onShellLine(mCommand)
            mProcess = Shizuku.newProcess(arrayOf("sh", "-c", mCommand), null, mDir)
            val mInput = BufferedReader(InputStreamReader(mProcess!!.getInputStream()))
            val mError = BufferedReader(InputStreamReader(mProcess!!.getErrorStream()))
            var line: String
            while ((mInput.readLine().also { line = it }) != null) {
                Log.i(shellTag, line)
                shellCallback?.onShellLine(line)
                mOutput.add(line)
            }
            while ((mError.readLine().also { line = it }) != null) {
                Log.e(shellTag, line)
                shellCallback?.onShellError(line)
                mOutput.add("<font color=#FF0000>$line</font>")
            }

            // Handle current directory
            if (mCommand.startsWith("cd ") && !mOutput[mOutput.size - 1]
                .endsWith("</font>")
            ) {
                val array: Array<String> =
                    mCommand.split("\\s+".toRegex()).dropLastWhile { it.isEmpty() }
                        .toTypedArray()
                var dir: String
                dir = if (array[array.size - 1] == "/") {
                    "/"
                } else if (array[array.size - 1].startsWith("/")) {
                    array[array.size - 1]
                } else {
                    mDir + array[array.size - 1]
                }
                if (!dir.endsWith("/")) {
                    dir = "$dir/"
                }
                mDir = dir
            }

            mProcess!!.waitFor()
        } catch (ignored: Exception) {
        }
        return this
    }

    fun destroy() {
        if (mProcess != null) mProcess!!.destroy()
    }

    companion object {
        private var mProcess: ShizukuRemoteProcess? = null
        private var mDir = "/"
    }
}