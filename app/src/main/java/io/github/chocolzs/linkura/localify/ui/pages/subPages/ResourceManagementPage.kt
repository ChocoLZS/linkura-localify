package io.github.chocolzs.linkura.localify.ui.pages.subPages

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.pulltorefresh.PullToRefreshBox
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentWidth
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.EmojiPeople
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.VideoFile
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.chocolzs.linkura.localify.MainActivity
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.getConfigState
import io.github.chocolzs.linkura.localify.mainUtils.ArchiveRepository
import io.github.chocolzs.linkura.localify.models.ArchiveItem
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.ReplaySettingsCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.ReplaySettingsCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.ui.components.ArchiveWaterfallGrid
import io.github.chocolzs.linkura.localify.ui.components.GakuGroupBox
import io.github.chocolzs.linkura.localify.ui.components.GakuSwitch
import io.github.chocolzs.linkura.localify.ui.components.GakuTabRow
import io.github.chocolzs.linkura.localify.ui.components.GakuTextInput
import io.github.chocolzs.linkura.localify.ui.components.base.CollapsibleBox
import kotlinx.coroutines.launch

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun ResourceManagementPage(
    modifier: Modifier = Modifier,
    context: MainActivity? = null,
    previewData: LinkuraConfig? = null,
    bottomSpacerHeight: Dp = 120.dp,
    screenH: Dp = 1080.dp
) {
    val tabTitles = listOf(stringResource(R.string.resource_management_replay))
    val pagerState = rememberPagerState(initialPage = 0, pageCount = { tabTitles.size })
    val coroutineScope = rememberCoroutineScope()

    Column(
        modifier = modifier
            .sizeIn(maxHeight = screenH)
            .fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        // Fixed Tab Bar at the top with custom styling
        Column(
            modifier = Modifier.fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // Tab bar with background and centered content
            Box(
                modifier = Modifier
                    .padding(horizontal = 4.dp, vertical = 2.dp)
            ) {
                TabRow(
                    selectedTabIndex = pagerState.currentPage,
                    modifier = Modifier.height(32.dp),
                    containerColor = Color.Transparent,
                    contentColor = MaterialTheme.colorScheme.onSurface,
                    indicator = { tabPositions ->
                        if (tabPositions.isNotEmpty()) {
                            Box(
                                Modifier
                                    .tabIndicatorOffset(tabPositions[pagerState.currentPage])
                                    .height(2.dp)
                                    .background(MaterialTheme.colorScheme.primary)
                            )
                        }
                    },
                    divider = {}
                ) {
                    tabTitles.forEachIndexed { index, title ->
                        Tab(
                            selected = pagerState.currentPage == index,
                            onClick = {
                                coroutineScope.launch {
                                    pagerState.scrollToPage(index)
                                }
                            },
                            modifier = Modifier
                                .height(32.dp)
                                .padding(horizontal = 8.dp)
                                .background(MaterialTheme.colorScheme.primary)
                                .wrapContentWidth()
                        ) {
                            Text(
                                text = title,
                                style = MaterialTheme.typography.labelMedium
                            )
                        }
                    }
                }
            }
        }

        HorizontalDivider(
            thickness = 2.dp,
            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
        )
        
        // Content with HorizontalPager
        HorizontalPager(
            state = pagerState,
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth(),
            pageSpacing = 10.dp
        ) { page ->
            when (page) {
                0 -> ReplayTabPage(
                    modifier = Modifier.fillMaxHeight(),
                    context = context,
                    previewData = previewData,
                    bottomSpacerHeight = bottomSpacerHeight,
                    screenH = screenH
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ReplayTabPage(
    modifier: Modifier = Modifier,
    context: MainActivity? = null,
    previewData: LinkuraConfig? = null,
    bottomSpacerHeight: Dp = 120.dp,
    screenH: Dp = 1080.dp
) {
    val config = getConfigState(context, previewData)
    val defaultMetadataUrl = stringResource(R.string.replay_default_metadata_url)

    var localMetadataUrl by remember { mutableStateOf(defaultMetadataUrl) }

    val replaySettingsViewModel: ReplaySettingsCollapsibleBoxViewModel =
        viewModel(factory = ReplaySettingsCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    
    // Separate state for icon legend collapsible
    var isIconLegendExpanded by remember { mutableStateOf(false) }

    // Archive data state
    var archiveList by remember { mutableStateOf<List<ArchiveItem>>(emptyList()) }
    var replayTypes by remember { mutableStateOf<Map<String, Int>>(emptyMap()) }
    var isLoadingArchives by remember { mutableStateOf(false) }
    var archiveError by remember { mutableStateOf<String?>(null) }

    // Pull to refresh state for ReplayTabPage
    var isRefreshing by remember { mutableStateOf(false) }

    // Fetch archive data function
    suspend fun fetchArchiveData() {
        if (localMetadataUrl.isBlank()) return

        isLoadingArchives = true
        archiveError = null

        try {
            val result = ArchiveRepository.fetchArchiveList(localMetadataUrl)
            result.onSuccess { fetchedList ->
                archiveList = fetchedList
                context?.let { ctx ->
                    ArchiveRepository.saveArchiveList(ctx, fetchedList)

                    val existingConfig = ArchiveRepository.loadArchiveConfig(ctx)
                    val newConfig =
                        ArchiveRepository.createArchiveConfigFromList(fetchedList, existingConfig)
                    ArchiveRepository.saveArchiveConfig(ctx, newConfig)

                    replayTypes = newConfig.associateBy({ it.archivesId }, { it.replayType })
                }
            }.onFailure { error ->
                archiveError = error.message ?: "Unknown error"
            }
        } finally {
            isLoadingArchives = false
        }
    }

    // Load initial archive data
    LaunchedEffect(Unit) {
        context?.let { ctx ->
            val savedArchives = ArchiveRepository.loadArchiveList(ctx)
            if (savedArchives != null) {
                archiveList = savedArchives
                val savedConfig = ArchiveRepository.loadArchiveConfig(ctx)
                replayTypes =
                    savedConfig?.associateBy({ it.archivesId }, { it.replayType }) ?: emptyMap()
            } else {
                // Auto-fetch if no data exists
                fetchArchiveData()
            }
        }
    }

    // Refresh function for replay settings
    suspend fun onRefresh() {
        isRefreshing = true
        try {
            fetchArchiveData()
        } finally {
            isRefreshing = false
        }
    }

    Column(
        modifier = modifier
            .sizeIn(maxHeight = screenH)
            .fillMaxWidth()
    ) {
        // Fixed GakuGroupBox at the top
        GakuGroupBox(
            Modifier
                .fillMaxWidth(),
            stringResource(R.string.replay_settings_title),
            contentPadding = 0.dp,
            onHeadClick = {
                replaySettingsViewModel.expanded = !replaySettingsViewModel.expanded
            }
        ) {
            CollapsibleBox(
                modifier = Modifier.fillMaxWidth(),
                viewModel = replaySettingsViewModel
            ) {
                Column(
                    modifier = Modifier.padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    // Enable Motion Capture Replay Switch
                    GakuSwitch(
                        text = stringResource(R.string.replay_settings_enable_motion_capture),
                        checked = config.value.enableMotionCaptureReplay,
                        onCheckedChange = { value ->
                            context?.onEnableMotionCaptureReplayChanged(value)
                        }
                    )

                    // In-Game Replay Display Sub-switch
                    GakuSwitch(
                        text = stringResource(R.string.replay_settings_enable_in_game_display),
                        checked = config.value.enableInGameReplayDisplay,
                        enabled = config.value.enableMotionCaptureReplay,
                        onCheckedChange = { value ->
                            context?.onEnableInGameReplayDisplayChanged(value)
                        }
                    )

                    // Description
                    Text(
                        text = stringResource(R.string.replay_settings_description),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                    )

                    // Motion Capture Resource URL Input with Reset Button
                    Column(
                        verticalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalAlignment = Alignment.Bottom
                        ) {
                            GakuTextInput(
                                value = config.value.motionCaptureResourceUrl,
                                onValueChange = { value ->
                                    context?.onMotionCaptureResourceUrlChanged(value)
                                },
                                modifier = Modifier.weight(1f),
                                label = {
                                    Text(text = stringResource(R.string.resource_url))
                                }
                            )

                            IconButton(
                                onClick = { 
                                    context?.onMotionCaptureResourceUrlChanged("https://assets.chocoie.com")
                                },
                                modifier = Modifier.size(48.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Refresh,
                                    contentDescription = stringResource(R.string.replay_settings_reset_url),
                                    tint = MaterialTheme.colorScheme.primary
                                )
                            }
                        }

                        // Display current effective URL
                        Text(
                            text = config.value.motionCaptureResourceUrl,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                            modifier = Modifier.padding(start = 16.dp)
                        )
                    }

                    // Replay Metadata URL Input with Reset Button
                    Column(
                        verticalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalAlignment = Alignment.Bottom
                        ) {
                            GakuTextInput(
                                value = localMetadataUrl,
                                onValueChange = { localMetadataUrl = it },
                                modifier = Modifier.weight(1f),
                                label = {
                                    Text(text = stringResource(R.string.replay_settings_metadata_url))
                                }
                            )

                            IconButton(
                                onClick = { localMetadataUrl = defaultMetadataUrl },
                                modifier = Modifier.size(48.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Refresh,
                                    contentDescription = stringResource(R.string.replay_settings_reset_url),
                                    tint = MaterialTheme.colorScheme.primary
                                )
                            }
                        }

                        // Display current effective URL
                        Text(
                            text = localMetadataUrl,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                            modifier = Modifier.padding(start = 16.dp)
                        )
                    }
                }
            }
        }

        Spacer(Modifier.height(16.dp))

        // Scrollable waterfall grid area with pull to refresh
        PullToRefreshBox(
            isRefreshing = isRefreshing,
            onRefresh = {
                CoroutineScope(Dispatchers.Main).launch {
                    onRefresh()
                }
            },
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth()
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 4.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                // Archive waterfall grid
                when {
                    isLoadingArchives -> {
                        Box(
                            modifier = Modifier.fillMaxWidth().padding(32.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                CircularProgressIndicator()
                                Spacer(Modifier.height(8.dp))
                                Text(
                                    text = stringResource(R.string.archive_loading),
                                    style = MaterialTheme.typography.bodyMedium
                                )
                            }
                        }
                    }

                    archiveError != null -> {
                        Box(
                            modifier = Modifier.fillMaxWidth().padding(32.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                                Text(
                                    text = stringResource(R.string.archive_error),
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = MaterialTheme.colorScheme.error
                                )
                                Spacer(Modifier.height(8.dp))
                                Text(
                                    text = archiveError!!,
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                                )
                                Spacer(Modifier.height(16.dp))
                                Button(
                                    onClick = {
                                        CoroutineScope(Dispatchers.Main).launch {
                                            fetchArchiveData()
                                        }
                                    }
                                ) {
                                    Text(stringResource(R.string.archive_retry))
                                }
                            }
                        }
                    }

                    archiveList.isEmpty() -> {
                        Box(
                            modifier = Modifier.fillMaxWidth().padding(32.dp),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(
                                text = stringResource(R.string.archive_empty),
                                style = MaterialTheme.typography.bodyMedium,
                                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                            )
                        }
                    }

                    else -> {
                        // Collapsible icon legend explanation
                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                        ) {
                            // Clickable header
                            Text(
                                text = stringResource(R.string.archive_icon_legend_title),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f),
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clickable {
                                        isIconLegendExpanded = !isIconLegendExpanded
                                    }
                            )
                            
                            // Collapsible content with animation
                            AnimatedVisibility(visible = isIconLegendExpanded) {
                                Column(
                                    modifier = Modifier
                                        .fillMaxWidth()
                                        .background(
                                            MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.3f),
                                            RoundedCornerShape(8.dp)
                                        )
                                        .padding(12.dp),
                                    verticalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                                    ) {
                                        Icon(
                                            imageVector = Icons.Filled.EmojiPeople,
                                            contentDescription = null,
                                            tint = Color(0xFF58D68E),
                                            modifier = Modifier.size(20.dp)
                                        )
                                        Text(
                                            text = stringResource(R.string.archive_motion_capture_official),
                                            style = MaterialTheme.typography.bodySmall,
                                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.8f)
                                        )
                                    }
                                    
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                                    ) {
                                        Icon(
                                            imageVector = Icons.Filled.EmojiPeople,
                                            contentDescription = null,
                                            tint = Color(0xFF816CC6), // Purple
                                            modifier = Modifier.size(20.dp)
                                        )
                                        Text(
                                            text = stringResource(R.string.archive_motion_capture_reconstructed),
                                            style = MaterialTheme.typography.bodySmall,
                                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.8f)
                                        )
                                    }
                                    
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                                    ) {
                                        Icon(
                                            imageVector = Icons.Filled.VideoFile,
                                            contentDescription = null,
                                            tint = Color(0xFF58D68E),
                                            modifier = Modifier.size(20.dp)
                                        )
                                        Text(
                                            text = stringResource(R.string.archive_video_replay),
                                            style = MaterialTheme.typography.bodySmall,
                                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.8f)
                                        )
                                    }
                                    
                                    Text(
                                        text = stringResource(R.string.archive_icon_legend_note),
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                                        modifier = Modifier.padding(top = 4.dp)
                                    )
                                }
                            }
                        }
                        
                        Spacer(Modifier.height(4.dp))
                        
                        ArchiveWaterfallGrid(
                            archiveList = archiveList,
                            replayTypes = replayTypes,
                            onReplayTypeToggle = { archivesId, newType ->
                                context?.let { ctx ->
                                    if (ArchiveRepository.updateArchiveConfigReplayType(
                                            ctx,
                                            archivesId,
                                            newType
                                        )
                                    ) {
                                        replayTypes = replayTypes.toMutableMap().apply {
                                            put(archivesId, newType)
                                        }
                                    }
                                }
                            }
                        )
                    }
                }

                Spacer(Modifier.height(bottomSpacerHeight))
            }
        }
    }
}

@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO, heightDp = 760)
@Composable
fun ResourceManagementPagePreview() {
    ResourceManagementPage(
        previewData = LinkuraConfig(
            enableMotionCaptureReplay = true,
            enableInGameReplayDisplay = false,
            motionCaptureResourceUrl = "https://assets.chocoie.com"
        )
    )
}