package io.github.chocolzs.linkura.localify.mainUtils

import android.content.Context
import android.util.Log
import io.github.chocolzs.linkura.localify.models.ArchiveConfig
import io.github.chocolzs.linkura.localify.models.ArchiveConfigList
import io.github.chocolzs.linkura.localify.models.ArchiveItem
import io.github.chocolzs.linkura.localify.models.ArchiveList
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.serialization.SerializationException
import kotlinx.serialization.encodeToString
import kotlinx.serialization.decodeFromString
import java.io.File
import java.net.HttpURLConnection
import java.net.URL

object AssetsRepository {
    private const val TAG = "AssetsRepository"
    private const val ARCHIVE_FILE = "archive.json"
    private const val ARCHIVE_CONFIG_FILE = "archive-config.json"
    private const val CLIENT_RES_FILE = "client-res.json"

    suspend fun fetchArchiveList(metadataUrl: String): Result<ArchiveList> = withContext(Dispatchers.IO) {
        try {
            val url = URL(metadataUrl)
            val connection = url.openConnection() as HttpURLConnection
            connection.requestMethod = "GET"
            connection.connectTimeout = 10000
            connection.readTimeout = 10000
            
            val responseCode = connection.responseCode
            if (responseCode == HttpURLConnection.HTTP_OK) {
                val jsonString = connection.inputStream.bufferedReader().use { it.readText() }
                val archiveList = json.decodeFromString<ArchiveList>(jsonString)
                Result.success(archiveList)
            } else {
                Result.failure(Exception("HTTP $responseCode"))
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to fetch archive list", e)
            Result.failure(e)
        }
    }

    fun saveArchiveList(context: Context, archiveList: ArchiveList): Boolean {
        return try {
            val file = File(context.filesDir, ARCHIVE_FILE)
            val jsonString = json.encodeToString<ArchiveList>(archiveList)
            file.writeText(jsonString)
            Log.d(TAG, "Archive list saved successfully")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save archive list", e)
            false
        }
    }

    fun loadArchiveList(context: Context): ArchiveList? {
        return try {
            val file = File(context.filesDir, ARCHIVE_FILE)
            if (file.exists()) {
                val jsonString = file.readText()
                json.decodeFromString<ArchiveList>(jsonString)
            } else {
                null
            }
        } catch (e: SerializationException) {
            Log.e(TAG, "Failed to load archive list", e)
            null
        }
    }

    fun saveArchiveConfig(context: Context, archiveConfigList: ArchiveConfigList): Boolean {
        return try {
            val file = File(context.filesDir, ARCHIVE_CONFIG_FILE)
            val jsonString = json.encodeToString<ArchiveConfigList>(archiveConfigList)
            file.writeText(jsonString)
            Log.d(TAG, "Archive config saved successfully")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save archive config", e)
            false
        }
    }

    fun loadArchiveConfig(context: Context): ArchiveConfigList? {
        return try {
            val file = File(context.filesDir, ARCHIVE_CONFIG_FILE)
            if (file.exists()) {
                val jsonString = file.readText()
                json.decodeFromString<ArchiveConfigList>(jsonString)
            } else {
                null
            }
        } catch (e: SerializationException) {
            Log.e(TAG, "Failed to load archive config", e)
            null
        }
    }

    fun createArchiveConfigFromList(archiveList: ArchiveList, existingConfig: ArchiveConfigList? = null): ArchiveConfigList {
        return archiveList.map { item ->
            val replayType = when {
                item.externalFixLink.isNotEmpty() -> 2  // external_fix_link 优先级最高
                item.externalLink.isNotEmpty() -> 1      // external_link 次之
                else -> 0                               // video_link 默认为0
            }
            
            ArchiveConfig(
                archivesId = item.archivesId,
                liveId = item.liveId,
                externalLink = item.externalLink,
                videoUrl = item.videoUrl,
                externalFixLink = item.externalFixLink,
                replayType = replayType,
                versionCompatibility = item.versionCompatibility
            )
        }
    }

    /**
     * 公共函数：处理 fetchArchiveList 后的保存逻辑
     * 刷新后不检测 existingConfig，直接进行覆盖
     * 创建规则：external_fix_link > external_link > video_link
     */
    suspend fun fetchAndSaveArchiveData(context: Context, metadataUrl: String): Result<ArchiveList> {
        return try {
            val result = fetchArchiveList(metadataUrl)
            result.onSuccess { fetchedList ->
                // Save archive list
                saveArchiveList(context, fetchedList)
                
                // Create new config without considering existing config (直接覆盖)
                val newConfig = createArchiveConfigFromList(fetchedList, null)
                saveArchiveConfig(context, newConfig)
                
                Log.d(TAG, "Archive data fetched and saved successfully: ${fetchedList.size} items")
            }
            result
        } catch (e: Exception) {
            Log.e(TAG, "Failed to fetch and save archive data", e)
            Result.failure(e)
        }
    }

    fun updateArchiveConfigReplayType(
        context: Context,
        archivesId: String,
        replayType: Int
    ): Boolean {
        val currentConfig = loadArchiveConfig(context) ?: return false
        val updatedConfig = currentConfig.map { config ->
            if (config.archivesId == archivesId) {
                config.copy(replayType = replayType)
            } else {
                config
            }
        }
        return saveArchiveConfig(context, updatedConfig)
    }

    suspend fun fetchClientRes(clientResUrl: String = "https://assets.chocoie.com/client-res"): Result<Map<String, List<String>>> = withContext(Dispatchers.IO) {
        try {
            val url = URL(clientResUrl)
            val connection = url.openConnection() as HttpURLConnection
            connection.requestMethod = "GET"
            connection.connectTimeout = 10000
            connection.readTimeout = 10000
            
            val responseCode = connection.responseCode
            if (responseCode == HttpURLConnection.HTTP_OK) {
                val jsonString = connection.inputStream.bufferedReader().use { it.readText() }
                val clientRes = json.decodeFromString<Map<String, List<String>>>(jsonString)
                Result.success(clientRes)
            } else {
                Result.failure(Exception("HTTP $responseCode"))
            }
        } catch (e: Exception) {
            Log.e(TAG, "Failed to fetch client resources", e)
            Result.failure(e)
        }
    }

    fun saveClientRes(context: Context, clientRes: Map<String, List<String>>): Boolean {
        return try {
            val file = File(context.filesDir, CLIENT_RES_FILE)
            val jsonString = json.encodeToString<Map<String, List<String>>>(clientRes)
            file.writeText(jsonString)
            Log.d(TAG, "Client resources saved successfully")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to save client resources", e)
            false
        }
    }

    fun loadClientRes(context: Context): Map<String, List<String>>? {
        return try {
            val file = File(context.filesDir, CLIENT_RES_FILE)
            if (file.exists()) {
                val jsonString = file.readText()
                json.decodeFromString<Map<String, List<String>>>(jsonString)
            } else {
                null
            }
        } catch (e: SerializationException) {
            Log.e(TAG, "Failed to load client resources", e)
            null
        }
    }

    fun getClientResForVersion(context: Context, version: String): List<String>? {
        val clientRes = loadClientRes(context)
        return clientRes?.get(version)
    }
}