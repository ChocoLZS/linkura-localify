package io.github.chocolzs.linkura.localify.ui.components

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.staggeredgrid.LazyVerticalStaggeredGrid
import androidx.compose.foundation.lazy.staggeredgrid.StaggeredGridCells
import androidx.compose.foundation.lazy.staggeredgrid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.EmojiPeople
import androidx.compose.material.icons.filled.VideoFile
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.foundation.MarqueeAnimationMode
import androidx.compose.foundation.basicMarquee
import coil.compose.AsyncImage
import coil.request.ImageRequest
import io.github.chocolzs.linkura.localify.models.ArchiveItem
import io.github.chocolzs.linkura.localify.models.ReplayType
import java.text.SimpleDateFormat
import java.util.*

fun Int.toReplayType(): ReplayType = when(this) {
    0 -> ReplayType.VIDEO
    1 -> ReplayType.MOTION_CAPTURE
    2 -> ReplayType.MOTION_CAPTURE_FIX
    else -> ReplayType.VIDEO
}

fun ReplayType.toInt(): Int = when(this) {
    ReplayType.VIDEO -> 0
    ReplayType.MOTION_CAPTURE -> 1
    ReplayType.MOTION_CAPTURE_FIX -> 2
}

fun getAvailableReplayTypes(item: ArchiveItem): List<ReplayType> {
    val availableTypes = mutableListOf<ReplayType>()
    
    if (item.videoUrl.isNotEmpty()) {
        availableTypes.add(ReplayType.VIDEO)
    }
    if (item.externalLink.isNotEmpty()) {
        availableTypes.add(ReplayType.MOTION_CAPTURE)
    }
    if (item.externalFixLink.isNotEmpty()) {
        availableTypes.add(ReplayType.MOTION_CAPTURE_FIX)
    }
    
    return availableTypes
}

fun cycleToNextReplayType(currentType: ReplayType, availableTypes: List<ReplayType>): ReplayType {
    if (availableTypes.size <= 1) return currentType
    
    val currentIndex = availableTypes.indexOf(currentType)
    val nextIndex = if (currentIndex == -1) 0 else (currentIndex + 1) % availableTypes.size
    return availableTypes[nextIndex]
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun ArchiveWaterfallGrid(
    archiveList: List<ArchiveItem>,
    replayTypes: Map<String, Int>,
    onReplayTypeToggle: (String, Int) -> Unit,
    modifier: Modifier = Modifier
) {
    LazyVerticalStaggeredGrid(
        columns = StaggeredGridCells.Fixed(2),
        modifier = modifier
            .fillMaxWidth()
            .heightIn(max = 600.dp), // Max height constraint to avoid infinite height
        horizontalArrangement = Arrangement.spacedBy(8.dp),
        verticalItemSpacing = 8.dp,
        contentPadding = PaddingValues(vertical = 8.dp)
    ) {
        items(archiveList) { item ->
            ArchiveGridItem(
                item = item,
                replayType = replayTypes[item.archivesId] ?: 1,
                onReplayTypeToggle = { 
                    val availableTypes = getAvailableReplayTypes(item)
                    val currentType = (replayTypes[item.archivesId] ?: 1).toReplayType()
                    val nextType = cycleToNextReplayType(currentType, availableTypes)
                    onReplayTypeToggle(item.archivesId, nextType.toInt())
                }
            )
        }
    }
}

@Composable
private fun ArchiveGridItem(
    item: ArchiveItem,
    replayType: Int,
    onReplayTypeToggle: () -> Unit,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        elevation = CardDefaults.cardElevation(defaultElevation = 4.dp),
        shape = RoundedCornerShape(8.dp)
    ) {
        Column(
            modifier = Modifier.fillMaxWidth()
                .background(MaterialTheme.colorScheme.primary)
        ) {
            // Image with overlays
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(16f / 9f)
            ) {
                // Thumbnail image
                AsyncImage(
                    model = ImageRequest.Builder(LocalContext.current)
                        .data(item.thumbnailImageUrl)
                        .crossfade(true)
                        .build(),
                    contentDescription = item.name,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.Crop
                )
                
                // Duration overlay (top right)
                val duration = calculateDuration(item.liveStartTime, item.liveEndTime)
                if (duration.isNotEmpty()) {
                    Box(
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .padding(4.dp)
                            .background(
                                Color.Black.copy(alpha = 0.7f),
                                RoundedCornerShape(4.dp)
                            )
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = duration,
                            color = Color.White,
                            style = MaterialTheme.typography.bodySmall
                        )
                    }
                }
                
                // Replay type badge (top left)
                val availableTypes = getAvailableReplayTypes(item)
                val canToggle = availableTypes.size > 1
                val currentReplayType = replayType.toReplayType()
                
                val badgeColor = when (currentReplayType) {
                    ReplayType.VIDEO, ReplayType.MOTION_CAPTURE -> Color(0xFF58D68E) // Green for video
                    ReplayType.MOTION_CAPTURE_FIX -> Color(0xFF816CC6) // Purple for motion capture with fix
                }
                
                Box(
                    modifier = Modifier
                        .align(Alignment.TopStart)
                        .padding(start = 8.dp, top = 0.dp)
                        .background(
                            if (canToggle) badgeColor else badgeColor.copy(alpha = 0.5f),
                            RoundedCornerShape(
                                topStart = 0.dp,
                                topEnd = 0.dp,
                                bottomStart = 8.dp,
                                bottomEnd = 8.dp
                            )
                        )
                        .padding(horizontal = 6.dp, vertical = 4.dp)
                        .then(
                            if (canToggle) {
                                Modifier.clickable {
                                    onReplayTypeToggle()
                                }
                            } else {
                                Modifier
                            }
                        )
                ) {
                    Icon(
                        imageVector = when (currentReplayType) {
                            ReplayType.VIDEO -> Icons.Default.VideoFile
                            ReplayType.MOTION_CAPTURE, ReplayType.MOTION_CAPTURE_FIX -> Icons.Default.EmojiPeople
                        },
                        contentDescription = when (currentReplayType) {
                            ReplayType.VIDEO -> "Video Replay"
                            ReplayType.MOTION_CAPTURE -> "Motion Capture Replay"
                            ReplayType.MOTION_CAPTURE_FIX -> "Motion Capture with Fix Replay"
                        },
                        tint = Color.White,
                        modifier = Modifier.size(16.dp)
                    )
                }
            }
            
            // Title (scrolling if too long)
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 2.dp, horizontal = 4.dp)
            ) {
                Text(
                    text = item.name,
                    style = MaterialTheme.typography.bodySmall,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.basicMarquee(
                        animationMode = MarqueeAnimationMode.Immediately,
                        initialDelayMillis = 500
                    )
                )
            }
        }
    }
}

private fun calculateDuration(startTime: String, endTime: String): String {
    return try {
        val format = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.getDefault())
        format.timeZone = TimeZone.getTimeZone("UTC")
        
        val start = format.parse(startTime)
        val end = format.parse(endTime)
        
        if (start != null && end != null) {
            val durationMs = end.time - start.time
            val hours = durationMs / (1000 * 60 * 60)
            val minutes = (durationMs % (1000 * 60 * 60)) / (1000 * 60)
            val seconds = (durationMs % (1000 * 60)) / 1000
            if (hours > 0)
                String.format("%02d:%02d:%02d", hours, minutes, seconds)
            else
                String.format("%02d:%02d", minutes, seconds)
        } else {
            ""
        }
    } catch (e: Exception) {
        ""
    }
}

private fun formatDate(dateTimeString: String): String {
    return try {
        val inputFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'", Locale.getDefault())
        inputFormat.timeZone = TimeZone.getTimeZone("UTC")
        
        val outputFormat = SimpleDateFormat("yyyy-MM-dd", Locale.getDefault())
        
        val date = inputFormat.parse(dateTimeString)
        if (date != null) {
            outputFormat.format(date)
        } else {
            ""
        }
    } catch (e: Exception) {
        ""
    }
}