package io.github.chocolzs.linkura.localify.models

import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable

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
    val videoUrl: String
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
    
    @SerialName("replay_type")
    val replayType: Int // 0 for video replay, 1 for motion capture replay
)

typealias ArchiveList = List<ArchiveItem>
typealias ArchiveConfigList = List<ArchiveConfig>