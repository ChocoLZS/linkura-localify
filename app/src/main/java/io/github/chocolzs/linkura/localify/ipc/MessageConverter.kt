package io.github.chocolzs.linkura.localify.ipc

import android.util.Log
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.mainUtils.json
import kotlinx.serialization.json.*

object MessageConverter {
    private const val TAG = "MessageConverter"
    
    fun jsonToCameraData(jsonString: String): CameraData? {
        return try {
            val jsonElement = json.parseToJsonElement(jsonString).jsonObject
            
            CameraData.newBuilder().apply {
                isValid = jsonElement["isValid"]?.jsonPrimitive?.boolean ?: false
                
                position = Vector3.newBuilder().apply {
                    x = jsonElement["position"]?.jsonObject?.get("x")?.jsonPrimitive?.float ?: 0f
                    y = jsonElement["position"]?.jsonObject?.get("y")?.jsonPrimitive?.float ?: 0f
                    z = jsonElement["position"]?.jsonObject?.get("z")?.jsonPrimitive?.float ?: 0f
                }.build()
                
                rotation = Quaternion.newBuilder().apply {
                    x = jsonElement["rotation"]?.jsonObject?.get("x")?.jsonPrimitive?.float ?: 0f
                    y = jsonElement["rotation"]?.jsonObject?.get("y")?.jsonPrimitive?.float ?: 0f
                    z = jsonElement["rotation"]?.jsonObject?.get("z")?.jsonPrimitive?.float ?: 0f
                    w = jsonElement["rotation"]?.jsonObject?.get("w")?.jsonPrimitive?.float ?: 1f
                }.build()
                
                fov = jsonElement["fov"]?.jsonPrimitive?.float ?: 60f
                mode = jsonElement["mode"]?.jsonPrimitive?.int ?: 0
                sceneType = jsonElement["sceneType"]?.jsonPrimitive?.int ?: 0
            }.build()
        } catch (e: Exception) {
            Log.e(TAG, "Error converting JSON to CameraData", e)
            null
        }
    }
    
    fun cameraDataToJson(cameraData: CameraData): String {
        return try {
            buildJsonObject {
                put("isValid", cameraData.isValid)
                put("position", buildJsonObject {
                    put("x", cameraData.position.x)
                    put("y", cameraData.position.y)
                    put("z", cameraData.position.z)
                })
                put("rotation", buildJsonObject {
                    put("x", cameraData.rotation.x)
                    put("y", cameraData.rotation.y)
                    put("z", cameraData.rotation.z)
                    put("w", cameraData.rotation.w)
                })
                put("fov", cameraData.fov)
                put("mode", cameraData.mode)
                put("sceneType", cameraData.sceneType)
            }.toString()
        } catch (e: Exception) {
            Log.e(TAG, "Error converting CameraData to JSON", e)
            "{}"
        }
    }
    
    fun parseCameraDataFromPayload(payload: ByteArray): CameraData? {
        return try {
            CameraData.parseFrom(payload)
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing CameraData from payload", e)
            null
        }
    }
}