package io.github.chocolzs.linkura.localify.ui.pages.subPages

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import android.util.Log
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
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import kotlinx.coroutines.delay
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.chocolzs.linkura.localify.MainActivity
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.getConfigState
import io.github.chocolzs.linkura.localify.getProgramConfigState
import io.github.chocolzs.linkura.localify.getProgramDownloadAbleState
import io.github.chocolzs.linkura.localify.getProgramDownloadErrorStringState
import io.github.chocolzs.linkura.localify.getProgramDownloadState
import io.github.chocolzs.linkura.localify.getProgramLocalAPIResourceVersionState
import io.github.chocolzs.linkura.localify.getProgramLocalResourceVersionState
import io.github.chocolzs.linkura.localify.hookUtils.FileHotUpdater
import io.github.chocolzs.linkura.localify.hookUtils.FilesChecker
import io.github.chocolzs.linkura.localify.mainUtils.AssetsRepository
import io.github.chocolzs.linkura.localify.mainUtils.FileDownloader
import io.github.chocolzs.linkura.localify.mainUtils.RemoteAPIFilesChecker
import io.github.chocolzs.linkura.localify.mainUtils.TimeUtils
import io.github.chocolzs.linkura.localify.models.ArchiveItem
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.LocaleItem
import io.github.chocolzs.linkura.localify.models.ReplaySettingsCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.ReplaySettingsCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.ResourceCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.ResourceCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.ui.components.ArchiveWaterfallGrid
import io.github.chocolzs.linkura.localify.ui.components.GakuButton
import io.github.chocolzs.linkura.localify.ui.components.GakuGroupBox
import io.github.chocolzs.linkura.localify.ui.components.GakuProgressBar
import io.github.chocolzs.linkura.localify.ui.components.GakuRadio
import io.github.chocolzs.linkura.localify.ui.components.GakuSelector
import io.github.chocolzs.linkura.localify.ui.components.GakuSwitch
import io.github.chocolzs.linkura.localify.ui.components.GakuTabRow
import io.github.chocolzs.linkura.localify.ui.components.GakuTextInput
import io.github.chocolzs.linkura.localify.ui.components.base.AutoSizeText
import io.github.chocolzs.linkura.localify.ui.components.base.CollapsibleBox
import kotlinx.coroutines.launch
import kotlinx.serialization.json.Json
import java.io.File
import java.io.IOException

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun ResourceManagementPage(
    modifier: Modifier = Modifier,
    context: MainActivity? = null,
    previewData: LinkuraConfig? = null,
    bottomSpacerHeight: Dp = 120.dp,
    screenH: Dp = 1080.dp
) {
    val config = getConfigState(context, previewData)
    val tabTitles = listOf(stringResource(R.string.resource_management_replay), stringResource(R.string.resource_management_locale))
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
                                .background(if (pagerState.currentPage == index) {MaterialTheme.colorScheme.primaryContainer} else {MaterialTheme.colorScheme.primary})
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
                1 -> LocaleTabPage(
                    modifier = Modifier.fillMaxHeight(),
                    context = context
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

    // Load saved metadata URL or use default
    var localMetadataUrl by remember { 
        mutableStateOf(
            context?.getSharedPreferences("linkura_prefs", 0)
                ?.getString("metadata_url", defaultMetadataUrl) ?: defaultMetadataUrl
        )
    }

    val replaySettingsViewModel: ReplaySettingsCollapsibleBoxViewModel =
        viewModel(factory = ReplaySettingsCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    
    // Separate state for icon legend collapsible
    var isIconLegendExpanded by remember { mutableStateOf(true) }

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
            context?.let { ctx ->
                val result = AssetsRepository.fetchAndSaveArchiveData(ctx, localMetadataUrl)
                result.onSuccess { fetchedList ->
                    archiveList = fetchedList
                    
                    // Load the newly created config
                    val newConfig = AssetsRepository.loadArchiveConfig(ctx)
                    replayTypes = newConfig?.associateBy({ it.archivesId }, { it.replayType }) ?: emptyMap()
                }.onFailure { error ->
                    archiveError = error.message ?: "Unknown error"
                }
            }
        } finally {
            isLoadingArchives = false
        }
    }

    // Fetch client resources function (silent)
    suspend fun fetchClientResources() {
        if (localMetadataUrl.isBlank()) return
        try {
            val result = AssetsRepository.fetchClientRes(localMetadataUrl)
            result.onSuccess { clientRes ->
                context?.let { ctx ->
                    AssetsRepository.saveClientRes(ctx, clientRes)
                }
            }
        } catch (e: Exception) {
            // Silent failure - no UI feedback needed
        }
    }

    // Load initial archive data and client resources
    LaunchedEffect(Unit) {
        context?.let { ctx ->
            val savedArchives = AssetsRepository.loadArchiveList(ctx)
            if (savedArchives != null) {
                archiveList = savedArchives
                val savedConfig = AssetsRepository.loadArchiveConfig(ctx)
                replayTypes =
                    savedConfig?.associateBy({ it.archivesId }, { it.replayType }) ?: emptyMap()
            } else {
                // Auto-fetch if no data exists
                fetchArchiveData()
            }
            
            // Also check and fetch client resources if not available
            val hasClientRes = AssetsRepository.loadClientRes(ctx) != null
            if (!hasClientRes) {
                fetchClientResources()
            }
        }
    }

    // Refresh function for replay settings
    suspend fun onRefresh() {
        isRefreshing = true
        try {
            // Fetch both archive data and client resources
            fetchArchiveData()
            fetchClientResources()
        } finally {
            isRefreshing = false
        }
    }

    // Pull to refresh now covers the entire content area
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
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(4.dp)
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
                    GakuSwitch(
                        text = stringResource(R.string.replay_settings_enable_custom_start_time),
                        checked = config.value.enableSetArchiveStartTime,
                        onCheckedChange = { value ->
                            context?.onEnableSetArchiveStartTimeChanged(value)
                        }
                    )

                    // Archive Start Time Input (enabled only if enableSetArchiveStartTime is true)
                    GakuTextInput(
                        value = config.value.archiveStartTime.toString(),
                        onValueChange = { value ->
                            val intValue = value.toIntOrNull() ?: 0
                            context?.onArchiveStartTimeChanged(intValue)
                        },
                        label = {
                            Text(text = stringResource(R.string.replay_settings_custom_start_time))
                        }
                    )

                    GakuSwitch(text = stringResource(R.string.config_legacy_title), checked = config.value.enableLegacyCompatibility) {
                            v -> context?.onEnableLegacyCompatibilityChanged(v)
                    }
                    // Note text for enableLegacyCompatibility
                    Text(
                        text = stringResource(R.string.config_legacy_description),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                    )

                    if (config.value.enableLegacyCompatibility) {
                        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                            Text(stringResource(R.string.config_legacy_resource_mode_title))
                            Row(modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                val radioModifier = Modifier.height(40.dp).weight(1f)

                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_legacy_resource_mode_default), selected = config.value.resourceVersionMode == 0,
                                    onClick = { context?.onResourceVersionModeChanged(0) })
                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_legacy_resource_mode_latest), selected = config.value.resourceVersionMode == 1,
                                    onClick = { context?.onResourceVersionModeChanged(1) })
                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_legacy_resource_mode_custom), selected = config.value.resourceVersionMode == 2,
                                    onClick = { context?.onResourceVersionModeChanged(2) })
                            }

                            if (config.value.resourceVersionMode == 2) {
                                Text(
                                    text = stringResource(R.string.config_legacy_resource_mode_custom_description),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.error.copy(alpha = 0.8f),
                                    modifier = Modifier.padding(horizontal = 4.dp)
                                )
                                var clientResMap by remember { mutableStateOf<Map<String, List<String>>>(emptyMap()) }
                                LaunchedEffect(Unit) {
                                    context?.let { ctx ->
                                        val loaded = AssetsRepository.loadClientRes(ctx)
                                        if (loaded != null) clientResMap = loaded
                                    }
                                }

                                if (clientResMap.isNotEmpty()) {
                                    val clientVersions = clientResMap.keys.toList()
                                    val selectedClientVer = if (config.value.customClientVersion.isNotEmpty() && clientResMap.containsKey(config.value.customClientVersion)) {
                                        config.value.customClientVersion
                                    } else {
                                        clientVersions.firstOrNull() ?: ""
                                    }

                                    // Update if empty
                                    if (config.value.customClientVersion.isEmpty() && selectedClientVer.isNotEmpty()) {
                                        context?.onCustomClientVersionChanged(selectedClientVer)
                                    }

                                    val resVersions = clientResMap[selectedClientVer] ?: emptyList()
                                    val selectedResVer = if (config.value.customResVersion.isNotEmpty() && resVersions.contains(config.value.customResVersion)) {
                                        config.value.customResVersion
                                    } else {
                                        resVersions.lastOrNull() ?: ""
                                    }
                                    
                                    // Update if empty
                                    if (config.value.customResVersion.isEmpty() && selectedResVer.isNotEmpty()) {
                                        context?.onCustomResVersionChanged(selectedResVer)
                                    }

                                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                        Text(stringResource(R.string.config_legacy_resource_mode_custom_client_ver))
                                        GakuSelector(
                                            options = clientVersions.map { it to it },
                                            selectedValue = selectedClientVer,
                                            onValueSelected = { ver ->
                                                context?.onCustomClientVersionChanged(ver)
                                                // Auto select last res version when client ver changes
                                                val newResList = clientResMap[ver]
                                                if (!newResList.isNullOrEmpty()) {
                                                    context?.onCustomResVersionChanged(newResList.last())
                                                }
                                            }
                                        )

                                        Text(stringResource(R.string.config_legacy_resource_mode_custom_res_ver))
                                        GakuSelector(
                                            options = resVersions.map { it to it },
                                            selectedValue = selectedResVer,
                                            onValueSelected = { ver ->
                                                context?.onCustomResVersionChanged(ver)
                                            }
                                        )
                                    }
                                } else {
                                    Text("No client resource data found. Please refresh archive data.")
                                }
                            }
                        }
                        
                        HorizontalDivider(
                            thickness = 1.dp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                        )
                    }

                    // Enable Motion Capture Replay Switch
                    GakuSwitch(
                        text = stringResource(R.string.replay_settings_enable_motion_capture),
                        checked = config.value.enableMotionCaptureReplay,
                        onCheckedChange = { value ->
                            context?.onEnableMotionCaptureReplayChanged(value)
                        }
                    )

                    GakuSwitch(
                        text = stringResource(R.string.replay_settings_avoid_accidental_touch),
                        checked = config.value.avoidAccidentalTouch,
                        enabled = config.value.enableMotionCaptureReplay,
                        onCheckedChange = { value ->
                            context?.onAvoidAccidentalTouchChanged(value)
                        }
                    )

                    GakuSwitch(
                        text = stringResource(R.string.replay_settings_filter_motion_capture_replay),
                        checked = config.value.filterMotionCaptureReplay,
                        enabled = config.value.enableMotionCaptureReplay,
                        onCheckedChange = { value ->
                            context?.onFilterMotionCaptureReplayChanged(value)
                        }
                    )

                    GakuSwitch(
                        text = stringResource(R.string.replay_settings_filter_playable_motion_capture),
                        checked = config.value.filterPlayableMotionCapture,
                        enabled = config.value.enableMotionCaptureReplay,
                        onCheckedChange = { value ->
                            context?.onFilterPlayableMotionCaptureChanged(value)
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
                                onValueChange = { newUrl ->
                                    localMetadataUrl = newUrl
                                    // Save to SharedPreferences
                                    context?.getSharedPreferences("linkura_prefs", 0)
                                        ?.edit()
                                        ?.putString("metadata_url", newUrl)
                                        ?.apply()
                                },
                                modifier = Modifier.weight(1f),
                                label = {
                                    Text(text = stringResource(R.string.replay_settings_metadata_url))
                                }
                            )

                            IconButton(
                                onClick = { 
                                    localMetadataUrl = defaultMetadataUrl
                                    // Save to SharedPreferences
                                    context?.getSharedPreferences("linkura_prefs", 0)
                                        ?.edit()
                                        ?.putString("metadata_url", defaultMetadataUrl)
                                        ?.apply()
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

            // Archive waterfall grid area
            Column(
                modifier = Modifier
                    .fillMaxWidth()
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
                                    if (AssetsRepository.updateArchiveConfigReplayType(
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun LocaleTabPage(
    modifier: Modifier = Modifier,
    context: MainActivity? = null,
    previewData: LinkuraConfig? = null,
    bottomSpacerHeight: Dp = 120.dp,
    screenH: Dp = 1080.dp
) {
    val config = getConfigState(context, previewData)
    val programConfig = getProgramConfigState(context)

    var isFirstTimeInThisPage by rememberSaveable { mutableStateOf(true) }
    val downloadProgress by getProgramDownloadState(context)
    val downloadAble by getProgramDownloadAbleState(context)
    val localResourceVersion by getProgramLocalResourceVersionState(context)
    val localAPIResourceVersion by getProgramLocalAPIResourceVersionState(context)
    val downloadErrorString by getProgramDownloadErrorStringState(context)
    val resourceSettingsViewModel: ResourceCollapsibleBoxViewModel =
        viewModel(factory = ResourceCollapsibleBoxViewModelFactory(initiallyExpanded = false))


    val locales = remember { mutableStateOf<List<LocaleItem>>(emptyList()) }

    fun zipResourceDownload() {
        val (_, newUrl) = FileDownloader.checkAndChangeDownloadURL(programConfig.value.transRemoteZipUrl)
        context?.onPTransRemoteZipUrlChanged(newUrl, 0, 0, 0)
        FileDownloader.downloadFile(
            newUrl,
            checkContentTypes = listOf("application/zip", "application/octet-stream"),
            onDownload = { progress, _, _ ->
                context?.mainPageAssetsViewDataUpdate(downloadProgressState = progress)
            },

            onSuccess = { byteArray ->
                context?.mainPageAssetsViewDataUpdate(
                    downloadAbleState = true,
                    errorString = "",
                    downloadProgressState = -1f
                )
                val file = File(context?.filesDir, "update_trans.zip")
                file.writeBytes(byteArray)
                val newFileVersion = FileHotUpdater.getZipResourceVersion(file.absolutePath)
                // TODO update i18n
                if (newFileVersion != null) {
                    context?.mainPageAssetsViewDataUpdate(
                        localResourceVersionState = newFileVersion
                    )
                }
                else {
                    context?.mainPageAssetsViewDataUpdate(
                        localResourceVersionState = context.getString(
                            R.string.invalid_zip_file
                        ),
                        errorString = context.getString(R.string.invalid_zip_file_warn)
                    )
                }
            },

            onFailed = { code, reason ->
                context?.mainPageAssetsViewDataUpdate(
                    downloadAbleState = true,
                    errorString = reason,
                )
            })
    }

    fun onClickDownload(isZipResource: Boolean, isHumanClick: Boolean = true) {
        context?.mainPageAssetsViewDataUpdate(
            downloadAbleState = false,
            errorString = "",
            downloadProgressState = -1f
        )
        if (isZipResource) {
            zipResourceDownload()
        }
        else {
            RemoteAPIFilesChecker.checkUpdateLocalAssets(context!!,
                programConfig.value.useAPIAssetsURL,
                onFailed = { _, reason ->
                    context.mainPageAssetsViewDataUpdate(
                        downloadAbleState = true,
                        errorString = "",
                        downloadProgressState = -1f
                    )
                    context.mainUIConfirmStatUpdate(true, "Error", reason)
                },
                onResult = { data, localVersion ->
                    if (!isHumanClick) {
                        if (data.tag_name == localVersion) {
                            context.mainPageAssetsViewDataUpdate(
                                downloadAbleState = true,
                                errorString = "",
                                downloadProgressState = -1f
                            )
                            return@checkUpdateLocalAssets
                        }
                    }
                    context.mainUIConfirmStatUpdate(true, context.getString(R.string.translation_resource_update),
                        "${data.name}\n$localVersion -> ${data.tag_name}\n${data.body}\n\n${TimeUtils.convertIsoToLocalTime(data.published_at)}",
                        onConfirm = {
                            resourceSettingsViewModel.expanded = true
                            RemoteAPIFilesChecker.updateLocalAssets(context, programConfig.value.useAPIAssetsURL,
                                onDownload = { progress, _, _ ->
                                    context.mainPageAssetsViewDataUpdate(downloadProgressState = progress)
                                },
                                onFailed = { _, reason -> context.mainPageAssetsViewDataUpdate(
                                    downloadAbleState = true,
                                    errorString = reason,
                                )},
                                onSuccess = { saveFile, releaseVersion ->
                                    context.mainPageAssetsViewDataUpdate(
                                        downloadAbleState = true,
                                        errorString = "",
                                        downloadProgressState = -1f
                                    )
                                    context.mainPageAssetsViewDataUpdate(
                                        localAPIResourceVersion = RemoteAPIFilesChecker.getLocalVersion(context)
                                    )
                                    context.saveProgramConfig()
                                    Log.d(TAG, "saved: $releaseVersion $saveFile")
                                })
                        },
                        onCancel = {
                            context.mainPageAssetsViewDataUpdate(
                                downloadAbleState = true,
                                errorString = "",
                                downloadProgressState = -1f
                            )
                        }
                    )
                })
        }
    }
    LaunchedEffect(context) {
        if (context != null) {
            try {
                val jsonString = context.assets.open("${FilesChecker.localizationFilesDir}/local-files/i18n.json").bufferedReader().use { it.readText() }
                locales.value = Json.decodeFromString<List<LocaleItem>>(jsonString)
            } catch (e: IOException) {
                Log.e(TAG, "Error reading i18n.json from assets", e)
            }
        }
    }
    LaunchedEffect(Unit) {
        try {
            if (context == null) return@LaunchedEffect
            val localAPIResVer = RemoteAPIFilesChecker.getLocalVersion(context)
            context.mainPageAssetsViewDataUpdate(
                localAPIResourceVersion = localAPIResVer
            )
            if (isFirstTimeInThisPage) {
                if (programConfig.value.useAPIAssets && programConfig.value.useAPIAssetsURL.isNotEmpty()) {
                    onClickDownload(false, false)
                }
            }
        }
        finally {
            isFirstTimeInThisPage = false
        }
    }
    Column(modifier = modifier
        .sizeIn(maxHeight = screenH)
        .fillMaxWidth()
        .verticalScroll(rememberScrollState()),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
            GakuGroupBox(
                Modifier.fillMaxWidth(),
                stringResource(R.string.resource_settings),
                contentPadding = 0.dp
            ) {
                LazyColumn(modifier = modifier
                    // .padding(8.dp)
                    .sizeIn(maxHeight = screenH),
                    // verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    item {
                        GakuSwitch(modifier.padding(horizontal = 8.dp), stringResource(R.string.replace_font), checked = config.value.replaceFont) {
                                v -> context?.onReplaceFontChanged(v)
                        }
                    }
                    item {
                        HorizontalDivider(
                            modifier = Modifier.padding(horizontal = 8.dp),
                            thickness = 1.dp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                        )
                    }
                    item {
                        GakuSwitch(modifier = modifier.padding(start = 8.dp, end = 8.dp, top = 8.dp),
                            checked = programConfig.value.usePluginBuiltInAssets,
                            text = stringResource(id = R.string.use_plugin_built_in_resource)
                        ) { v -> context?.onPUsePluginBuiltInAssetsChanged(v) }
                        Text(
                            text = "Useful for development",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                        )
                    }
                    item {
                        HorizontalDivider(
                            thickness = 1.dp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                        )
                    }

                    item {
                        GakuSwitch(modifier = modifier.padding(start = 8.dp, end = 8.dp),
                            checked = programConfig.value.useAPIAssets,
                            text = stringResource(R.string.check_resource_from_api)
                        ) { v -> context?.onPUseAPIAssetsChanged(v) }

                        CollapsibleBox(modifier = modifier.graphicsLayer(clip = false),
                            expandState = programConfig.value.useAPIAssets,
                            collapsedHeight = 0.dp,
                            innerPaddingLeftRight = 8.dp,
                            showExpand = false
                        ) {
                            GakuSwitch(modifier = modifier,
                                checked = programConfig.value.delRemoteAfterUpdate,
                                text = stringResource(id = R.string.del_remote_after_update)
                            ) { v -> context?.onPDelRemoteAfterUpdateChanged(v) }

                            LazyColumn(modifier = modifier
                                // .padding(8.dp)
                                .sizeIn(maxHeight = screenH),
                                verticalArrangement = Arrangement.spacedBy(12.dp)
                            ) {
                                item {
                                    Row(modifier = modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.spacedBy(2.dp),
                                        verticalAlignment = Alignment.CenterVertically) {

                                        GakuTextInput(modifier = modifier
                                            .height(45.dp)
                                            .padding(end = 8.dp)
                                            .fillMaxWidth()
                                            .weight(1f),
                                            fontSize = 14f,
                                            value = programConfig.value.useAPIAssetsURL,
                                            onValueChange = { c -> context?.onPUseAPIAssetsURLChanged(c, 0, 0, 0)},
                                            label = { Text(stringResource(R.string.api_addr)) }
                                        )

                                        if (downloadAble) {
                                            GakuButton(modifier = modifier
                                                .height(40.dp)
                                                .sizeIn(minWidth = 80.dp),
                                                text = stringResource(R.string.check_update),
                                                onClick = { onClickDownload(false) })
                                        }
                                        else {
                                            GakuButton(modifier = modifier
                                                .height(40.dp)
                                                .sizeIn(minWidth = 80.dp),
                                                text = stringResource(id = R.string.cancel), onClick = {
                                                    FileDownloader.cancel()
                                                })
                                        }

                                    }
                                }

                                if (downloadProgress >= 0) {
                                    item {
                                        GakuProgressBar(progress = downloadProgress, isError = downloadErrorString.isNotEmpty())
                                    }
                                }

                                if (downloadErrorString.isNotEmpty()) {
                                    item {
                                        Text(text = downloadErrorString, color = Color(0xFFE2041B))
                                    }
                                }

                                item {
                                    Text(modifier = Modifier
                                        .fillMaxWidth()
                                        .clickable {
                                            context?.mainPageAssetsViewDataUpdate(
                                                localAPIResourceVersion = RemoteAPIFilesChecker.getLocalVersion(
                                                    context
                                                )
                                            )
                                        }, text = "${stringResource(R.string.downloaded_resource_version)}: $localAPIResourceVersion")
                                }

                                item {
                                    Spacer(Modifier.height(0.dp))
                                }

                            }

                        }
                    }

                    item {
                        HorizontalDivider(
                            thickness = 1.dp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                        )
                    }

                    item {
                        GakuSwitch(modifier = modifier.padding(start = 8.dp, end = 8.dp),
                            checked = programConfig.value.useRemoteAssets,
                            text = stringResource(id = R.string.use_remote_zip_resource)
                        ) { v -> context?.onPUseRemoteAssetsChanged(v) }

                        CollapsibleBox(modifier = modifier.graphicsLayer(clip = false),
                            expandState = programConfig.value.useRemoteAssets,
                            collapsedHeight = 0.dp,
                            innerPaddingLeftRight = 8.dp,
                            showExpand = false
                        ) {
                            GakuSwitch(modifier = modifier,
                                checked = programConfig.value.delRemoteAfterUpdate,
                                text = stringResource(id = R.string.del_remote_after_update)
                            ) { v -> context?.onPDelRemoteAfterUpdateChanged(v) }

                            LazyColumn(modifier = modifier
                                // .padding(8.dp)
                                .sizeIn(maxHeight = screenH),
                                verticalArrangement = Arrangement.spacedBy(12.dp)
                            ) {
                                item {
                                    Row(modifier = modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.spacedBy(2.dp),
                                        verticalAlignment = Alignment.CenterVertically) {

                                        GakuTextInput(modifier = modifier
                                            .height(45.dp)
                                            .padding(end = 8.dp)
                                            .fillMaxWidth()
                                            .weight(1f),
                                            fontSize = 14f,
                                            value = programConfig.value.transRemoteZipUrl,
                                            onValueChange = { c -> context?.onPTransRemoteZipUrlChanged(c, 0, 0, 0)},
                                            label = { Text(stringResource(id = R.string.resource_url)) }
                                        )

                                        if (downloadAble) {
                                            GakuButton(modifier = modifier
                                                .height(40.dp)
                                                .sizeIn(minWidth = 80.dp),
                                                text = stringResource(id = R.string.download),
                                                onClick = { onClickDownload(true) })
                                        }
                                        else {
                                            GakuButton(modifier = modifier
                                                .height(40.dp)
                                                .sizeIn(minWidth = 80.dp),
                                                text = stringResource(id = R.string.cancel), onClick = {
                                                    FileDownloader.cancel()
                                                })
                                        }

                                    }
                                }

                                if (downloadProgress >= 0) {
                                    item {
                                        GakuProgressBar(progress = downloadProgress, isError = downloadErrorString.isNotEmpty())
                                    }
                                }

                                if (downloadErrorString.isNotEmpty()) {
                                    item {
                                        Text(text = downloadErrorString, color = Color(0xFFE2041B))
                                    }
                                }
                                if (programConfig.value.useRemoteAssets) {
                                    item {
                                        val file =
                                            File(context?.filesDir, "update_trans.zip")
                                        locales.value = FileHotUpdater.getZipI18nData(file.absolutePath)
                                        context?.mainPageAssetsViewDataUpdate(
                                            localResourceVersionState = FileHotUpdater
                                                .getZipResourceVersion(file.absolutePath)
                                                .toString()
                                        )
                                        Text(modifier = Modifier
                                            .fillMaxWidth()
                                            , text = "${stringResource(R.string.downloaded_resource_version)}: $localResourceVersion")
                                    }
                                }


                                item {
                                    Spacer(Modifier.height(0.dp))
                                }

                            }

                        }
                    }
                    item {
                        HorizontalDivider(
                            thickness = 1.dp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                        )
                    }
//                    item {
//                        GakuSwitch(modifier = modifier.padding(start = 8.dp, end = 8.dp),
//                            checked = programConfig.value.cleanLocalAssets,
//                            text = stringResource(id = R.string.delete_plugin_resource)
//                        ) { v -> context?.onPCleanLocalAssetsChanged(v) }
//                    }
//                    item {
//                        HorizontalDivider(
//                            thickness = 1.dp,
//                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
//                        )
//                    }
                    item {
                        Row(
                            modifier = modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            AutoSizeText(text = stringResource(R.string.resource_management_locale_language), fontSize = 16.sp)
                            GakuSelector (
                                options = listOf("日本語" to "ja-JP") + locales.value.map { it.name to it.code },
                                selectedValue = config.value.localeCode,
                                onValueSelected = { code ->
                                    context?.onLocaleCodeChanged(code)
                                }
                            )
                        }
                    }
                }
            }

            Spacer(Modifier.height(6.dp))

        Spacer(Modifier.height(bottomSpacerHeight))
    }
}