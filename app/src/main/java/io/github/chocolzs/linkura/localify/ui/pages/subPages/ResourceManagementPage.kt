package io.github.chocolzs.linkura.localify.ui.pages.subPages

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
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
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Refresh
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
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.ReplaySettingsCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.ReplaySettingsCollapsibleBoxViewModelFactory
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
    
    // Pull to refresh state for ReplayTabPage
    var isRefreshing by remember { mutableStateOf(false) }
    
    // Refresh function for replay settings
    suspend fun onRefresh() {
        isRefreshing = true
        try {
            // 模拟刷新操作
            delay(1000)
            // TODO: 在这里添加实际的回放相关刷新逻辑
            // 例如：重新加载回放资源、检查资源URL状态、重置默认值等
            localMetadataUrl = defaultMetadataUrl
        } finally {
            isRefreshing = false
        }
    }

    PullToRefreshBox(
        isRefreshing = isRefreshing,
        onRefresh = { 
            CoroutineScope(Dispatchers.Main).launch {
                onRefresh()
            }
        },
        modifier = modifier
            .sizeIn(maxHeight = screenH)
            .fillMaxWidth()
    ) {
        LazyColumn(
            modifier = Modifier.fillMaxWidth(),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            item {
                GakuGroupBox(
                    modifier, 
                    stringResource(R.string.replay_settings_title),
                    contentPadding = 0.dp,
                    onHeadClick = {
                        replaySettingsViewModel.expanded = !replaySettingsViewModel.expanded
                    }
                ) {
                    CollapsibleBox(
                        modifier = modifier,
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

                            // Motion Capture Resource URL Input
                            GakuTextInput(
                                value = config.value.motionCaptureResourceUrl,
                                onValueChange = { value ->
                                    context?.onMotionCaptureResourceUrlChanged(value)
                                },
                                modifier = Modifier.fillMaxWidth(),
                                label = {
                                    Text(text = stringResource(R.string.resource_url))
                                }
                            )

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