package io.github.chocolzs.linkura.localify.mainUtils

import android.content.Context
import android.util.Log
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.models.GithubReleaseModel
import kotlinx.serialization.json.Json
import okhttp3.Call
import okhttp3.Callback
import okhttp3.Request
import okhttp3.Response
import java.io.File
import java.io.IOException


object RemoteAPIFilesChecker {
    const val BASEPATH = "remote_files"

    fun getLocalVersion(context: Context): String? {
        val basePath = File(context.filesDir, BASEPATH)
        val versionFile = File(basePath, "version.txt")
        if (!versionFile.exists()) {
            return null
        }
        return versionFile.readText()
    }

    // version.txt in zip file should be same with version parameter
    fun saveDownloadData(context: Context, data: ByteArray, version: String): File {
        val basePath = File(context.filesDir, BASEPATH)
        if (!basePath.exists()) {
            basePath.mkdirs()
        }
        val versionFile = File(basePath, "version.txt")
        val dataFile = File(basePath, "remote.zip")
        dataFile.writeBytes(data)
        versionFile.writeText(version)
        return dataFile
    }

    fun checkUpdateLocalAssets(context: Context, apiURL: String,
                               onFailed: (Int, String) -> Unit,
                               onResult: (data: GithubReleaseModel, localVersion: String?) -> Unit) {
        runCatching {
            val request = Request.Builder()
                .url(apiURL)
                .build()
            FileDownloader.requestGet(request, object : Callback {
                override fun onFailure(call: Call, e: IOException) {
                    onFailed(-1, e.toString())
                }

                override fun onResponse(call: Call, response: Response) {
                    runCatching {
                        response.use {
                            if (!response.isSuccessful) throw IOException("Unexpected code $response")

                            val responseBody = response.body?.string()
                            if (responseBody != null) {
                                val json = Json { ignoreUnknownKeys = true }
                                val releaseData = json.decodeFromString<GithubReleaseModel>(responseBody)

                                // Check update
                                // val releaseVersion = releaseData.tag_name
                                val localVersion = getLocalVersion(context)
                                // if (releaseVersion != localVersion) {
                                onResult(releaseData, localVersion)
                                // }
                            } else {
                                onFailed(-1, "Response body is null")
                            }
                        }
                    }.onFailure { e ->
                        Log.e(TAG, "checkUpdateLocalAssets failed", e)
                        onFailed(-1, e.toString())
                    }
                }
            })
        }.onFailure { e ->
            Log.e(TAG, "checkUpdateLocalAssets failed", e)
            onFailed(-1, e.toString())
        }

    }

    fun updateLocalAssets(context: Context, apiURL: String,
                          onDownload: (Float, downloaded: Long, size: Long) -> Unit,
                          onFailed: (Int, String) -> Unit,
                          onSuccess: (File, String) -> Unit) {
        runCatching {
            val request = Request.Builder()
                .url(apiURL)
                .build()
            FileDownloader.requestGet(request, object : Callback {
                override fun onFailure(call: Call, e: IOException) {
                    onFailed(-1, e.toString())
                }

                override fun onResponse(call: Call, response: Response) {
                    runCatching {
                        response.use {
                            if (!response.isSuccessful) throw IOException("Unexpected code $response")

                            val responseBody = response.body?.string()
                            if (responseBody != null) {
                                val json = Json { ignoreUnknownKeys = true }
                                val releaseData = json.decodeFromString<GithubReleaseModel>(responseBody)

                                // Check and save update
                                val releaseVersion = releaseData.tag_name
                                val localVersion = getLocalVersion(context)
                                if (releaseVersion != localVersion) {
                                    for (asset in releaseData.assets) {
                                        if (!asset.name.endsWith(".zip")) continue
                                        FileDownloader.downloadFile(asset.browser_download_url,
                                            onDownload, {data ->
                                                runCatching {
                                                    val saveFile = saveDownloadData(context, data, releaseVersion)
                                                    onSuccess(saveFile, releaseVersion)
                                                }.onFailure { e ->
                                                    onFailed(-1, e.toString())
                                                }
                                            },
                                            onFailed)
                                        break
                                    }
                                }
                            } else {
                                onFailed(-1, "Response body is null")
                            }
                        }
                    }.onFailure { e ->
                        Log.e(TAG, "updateLocalAssets failed", e)
                        onFailed(-1, e.toString())
                    }
                }
            })
        }.onFailure { e ->
            Log.e(TAG, "updateLocalAssets failed", e)
            onFailed(-1, e.toString())
        }
    }

}