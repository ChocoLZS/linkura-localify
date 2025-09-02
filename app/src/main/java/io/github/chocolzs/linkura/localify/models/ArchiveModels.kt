package io.github.chocolzs.linkura.localify.models

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

enum class ReplayType {
    VIDEO,
    MOTION_CAPTURE,
    MOTION_CAPTURE_FIX
}

@Serializable
data class VersionCompatibility(
    @SerialName("rule")
    val rule: String,
    
    @SerialName("message")
    val message: String
)

@Serializable
data class ArchiveItem(
    @SerialName("archives_id")
    val archivesId: String,
    
    @SerialName("description")
    val description: String,
    
    @SerialName("external_link")
    val externalLink: String,
    
    @SerialName("live_end_time")
    val liveEndTime: String,
    
    @SerialName("live_id")
    val liveId: String,
    
    @SerialName("live_start_time")
    val liveStartTime: String,
    
    @SerialName("live_type")
    val liveType: Int,
    
    @SerialName("name")
    val name: String,
    
    @SerialName("thumbnail_image_url")
    val thumbnailImageUrl: String,
    
    @SerialName("video_url")
    val videoUrl: String,

    @SerialName("external_fix_link")
    val externalFixLink: String = "",

    @SerialName("version_compatibility")
    val versionCompatibility: VersionCompatibility? = null
)

@Serializable
data class ArchiveConfig(
    @SerialName("archives_id")
    val archivesId: String,
    
    @SerialName("live_id")
    val liveId: String,
    
    @SerialName("external_link")
    val externalLink: String,
    
    @SerialName("video_url")
    val videoUrl: String,

    @SerialName("external_fix_link")
    val externalFixLink: String = "",
    
    @SerialName("replay_type")
    val replayType: Int, // 0 for video replay, 1 for motion capture replay, 2 for motion capture with fix
    
    @SerialName("version_compatibility")
    val versionCompatibility: VersionCompatibility? = null
)

typealias ArchiveList = List<ArchiveItem>
typealias ArchiveConfigList = List<ArchiveConfig>