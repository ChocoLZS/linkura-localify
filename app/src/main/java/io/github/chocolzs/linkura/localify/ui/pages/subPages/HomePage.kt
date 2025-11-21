package io.github.chocolzs.linkura.localify.ui.pages.subPages

import io.github.chocolzs.linkura.localify.ui.components.GakuGroupBox
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import android.util.Log
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.chocolzs.linkura.localify.models.LocaleItem
import io.github.chocolzs.linkura.localify.ui.components.GakuSelector
import java.io.IOException
import kotlinx.serialization.json.Json
import io.github.chocolzs.linkura.localify.MainActivity
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.TAG
import io.github.chocolzs.linkura.localify.getConfigState
import io.github.chocolzs.linkura.localify.getProgramConfigState
import io.github.chocolzs.linkura.localify.getProgramDownloadAbleState
import io.github.chocolzs.linkura.localify.getProgramDownloadErrorStringState
import io.github.chocolzs.linkura.localify.getProgramDownloadState
import io.github.chocolzs.linkura.localify.getProgramLocalResourceVersionState
import io.github.chocolzs.linkura.localify.getProgramLocalAPIResourceVersionState
import io.github.chocolzs.linkura.localify.hookUtils.FileHotUpdater
import io.github.chocolzs.linkura.localify.hookUtils.FilesChecker
import io.github.chocolzs.linkura.localify.mainUtils.FileDownloader
import io.github.chocolzs.linkura.localify.mainUtils.RemoteAPIFilesChecker
import io.github.chocolzs.linkura.localify.mainUtils.TimeUtils
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.models.ResourceCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.ResourceCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.ui.components.base.CollapsibleBox
import io.github.chocolzs.linkura.localify.ui.components.GakuButton
import io.github.chocolzs.linkura.localify.ui.components.GakuProgressBar
import io.github.chocolzs.linkura.localify.ui.components.GakuRadio
import io.github.chocolzs.linkura.localify.ui.components.GakuSwitch
import io.github.chocolzs.linkura.localify.ui.components.GakuTextInput
import io.github.chocolzs.linkura.localify.ui.components.base.AutoSizeText
import java.io.File


@Composable
fun HomePage(modifier: Modifier = Modifier,
             context: MainActivity? = null,
             previewData: LinkuraConfig? = null,
             bottomSpacerHeight: Dp = 120.dp,
             screenH: Dp = 1080.dp) {
    val config = getConfigState(context, previewData)
    val programConfig = getProgramConfigState(context)

    // val scrollState = rememberScrollState()
    val keyboardOptionsNumber = remember {
        KeyboardOptions(keyboardType = KeyboardType.Number)
    }
    val keyBoardOptionsDecimal = remember {
        KeyboardOptions(keyboardType = KeyboardType.Decimal)
    }

    LazyColumn(modifier = modifier
        .sizeIn(maxHeight = screenH)
        // .fillMaxHeight()
        // .verticalScroll(scrollState)
        // .width(IntrinsicSize.Max)
        .fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        item {
            GakuGroupBox(modifier = modifier, stringResource(R.string.basic_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.enable_plugin), checked = config.value.enabled) {
                            v -> context?.onEnabledChanged(v)
                    }

                    GakuSwitch(modifier, stringResource(R.string.lazy_init), checked = config.value.lazyInit) {
                            v -> context?.onLazyInitChanged(v)
                    }

                    GakuSwitch(modifier, stringResource(R.string.app_update_check), checked = programConfig.value.checkAppUpdate) {
                            v -> context?.onPCheckAppUpdateChanged(v)
                    }

                }
            }
            Spacer(Modifier.height(6.dp))
        }
        item {
            GakuGroupBox(modifier = modifier, "Settings") {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuTextInput(
                        value = config.value.assetsUrlPrefix,
                        onValueChange = { value ->
                            context?.onAssetsUrlPrefixChanged(value)
                        },
                        label = {
                            Text(text = stringResource(R.string.config_assets_update_assets_url_prefix))
                        }
                    )
                    Text(
                        text = stringResource(R.string.config_assets_update_assets_url_prefix_placeholder),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                    )
                }
            }
            Spacer(Modifier.height(6.dp))
        }
        item {
            GakuGroupBox(modifier = modifier, contentPadding = 12.dp, title = stringResource(R.string.graphic_settings)) {
                LazyColumn(modifier = Modifier
                    .sizeIn(maxHeight = screenH),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    item {
                        GakuSwitch(modifier, stringResource(R.string.config_render_highResolution_title), checked = config.value.renderHighResolution) {
                                v -> context?.onRenderHighResolutionChanged(v)
                        }
                    }
                    item {
                        Spacer(modifier = Modifier.height(8.dp))
                        GakuTextInput(modifier = modifier
                            .padding(start = 4.dp, end = 4.dp)
                            .height(45.dp)
                            .fillMaxWidth(),
                            fontSize = 14f,
                            value = config.value.targetFrameRate.toString(),
                            onValueChange = { c -> context?.onTargetFpsChanged(c, 0, 0, 0)},
                            label = { Text(stringResource(R.string.setFpsTitle)) },
                            keyboardOptions = keyboardOptionsNumber)
                    }

                    item {
                        Column(modifier = Modifier.padding(start = 8.dp, end = 8.dp),
                            verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            Text(stringResource(R.string.orientation_lock))
                            Row(modifier = modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                                val radioModifier = remember {
                                    modifier
                                        .height(40.dp)
                                        .weight(1f)
                                }

                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.orientation_orig), selected = config.value.withliveOrientation == 2,
                                    onClick = { context?.onWithliveOrientationChanged(2) })
                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.orientation_landscape), selected = config.value.withliveOrientation == 0,
                                    onClick = { context?.onWithliveOrientationChanged(0) })
                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.orientation_portrait), selected = config.value.withliveOrientation == 1,
                                    onClick = { context?.onWithliveOrientationChanged(1) })

                            }
                        }
                    }

                    item {
                        GakuSwitch(modifier.padding(start = 8.dp, end = 8.dp),
                            stringResource(R.string.config_render_texture_lock_resolution_title),
                            checked = config.value.lockRenderTextureResolution) {
                                v -> context?.onLockRenderTextureResolutionChanged(v)
                        }

                        CollapsibleBox(modifier = modifier,
                            expandState = config.value.lockRenderTextureResolution,
                            collapsedHeight = 0.dp,
                            showExpand = false
                        ) {
                            Column(modifier = Modifier.padding(8.dp),
                                verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                
                                // Preset selector and resolution inputs
                                var selectedPreset by remember { mutableStateOf("custom") }
                                var longSideText by remember { mutableStateOf(config.value.renderTextureLongSide.toString()) }
                                var shortSideText by remember { mutableStateOf(config.value.renderTextureShortSide.toString()) }
                                var isDropdownExpanded by remember { mutableStateOf(false) }
                                
                                // Arrow rotation animation
                                val arrowRotation by animateFloatAsState(
                                    targetValue = if (isDropdownExpanded) 180f else 0f,
                                    animationSpec = tween(durationMillis = 300),
                                    label = "arrow_rotation"
                                )

                                // Update selectedPreset when config changes
                                LaunchedEffect(config.value.renderTextureLongSide, config.value.renderTextureShortSide) {
                                    val currentLong = config.value.renderTextureLongSide
                                    val currentShort = config.value.renderTextureShortSide
                                    selectedPreset = when {
                                        currentLong == 7680 && currentShort == 4320 -> "8k"
                                        currentLong == 3840 && currentShort == 2160 -> "4k"
                                        currentLong == 2560 && currentShort == 1440 -> "2k"
                                        currentLong == 1920 && currentShort == 1080 -> "1080p"
                                        currentLong == 1280 && currentShort == 720 -> "720p"
                                        currentLong == 640 && currentShort == 360 -> "360p"
                                        else -> "custom"
                                    }
                                    longSideText = currentLong.toString()
                                    shortSideText = currentShort.toString()
                                }

                                val presetOptions = listOf(
                                    stringResource(R.string.config_render_texture_preset_8k) to "8k",
                                    stringResource(R.string.config_render_texture_preset_4k) to "4k",
                                    stringResource(R.string.config_render_texture_preset_2k) to "2k",
                                    stringResource(R.string.config_render_texture_preset_1080p) to "1080p",
                                    stringResource(R.string.config_render_texture_preset_720p) to "720p",
                                    stringResource(R.string.config_render_texture_preset_360p) to "360p",
                                    stringResource(R.string.config_render_texture_preset_custom) to "custom"
                                )
                                Row(modifier = modifier.fillMaxWidth(),
                                    horizontalArrangement = Arrangement.spacedBy(4.dp),
                                    verticalAlignment = Alignment.CenterVertically) {
                                    GakuSelector(
                                        options = presetOptions,
                                        selectedValue = selectedPreset,
                                        onValueSelected = { preset ->
                                            selectedPreset = preset

                                            val (newLong, newShort) = when(preset) {
                                                "8k" -> Pair(7680, 4320)
                                                "4k" -> Pair(3840, 2160)
                                                "2k" -> Pair(2560, 1440)
                                                "1080p" -> Pair(1920, 1080)
                                                "720p" -> Pair(1280, 720)
                                                "360p" -> Pair(640, 360)
                                                else -> Pair(longSideText.toIntOrNull() ?: 3840, shortSideText.toIntOrNull() ?: 2160)
                                            }

                                            if (preset != "custom") {
                                                longSideText = newLong.toString()
                                                shortSideText = newShort.toString()
                                                context?.onRenderTextureResolutionChanged(newLong, newShort)
                                            }
                                        }
                                    )

                                    // Resolution inputs: longSide x shortSide
                                    GakuTextInput(modifier = Modifier
                                        .height(32.dp)
                                        .weight(.8f),
                                        fontSize = 12f,
                                        value = longSideText,
                                        onValueChange = { newValue ->
                                            longSideText = newValue
                                            val longSide = newValue.toIntOrNull()
                                            if (longSide != null && longSide > 0) {
                                                val shortSide = (longSide * 9 / 16).coerceAtLeast(1)
                                                shortSideText = shortSide.toString()
                                                context?.onRenderTextureResolutionChanged(longSide, shortSide)
                                            }
                                        },
                                        keyboardOptions = keyboardOptionsNumber)

                                    Text("×", modifier = Modifier.padding(horizontal = 4.dp))

                                    GakuTextInput(modifier = Modifier
                                        .height(32.dp)
                                        .weight(.8f),
                                        fontSize = 12f,
                                        value = shortSideText,
                                        onValueChange = { newValue ->
                                            shortSideText = newValue
                                            val shortSide = newValue.toIntOrNull()
                                            if (shortSide != null && shortSide > 0) {
                                                val longSide = (shortSide * 16 / 9).coerceAtLeast(1)
                                                longSideText = longSide.toString()
                                                context?.onRenderTextureResolutionChanged(longSide, shortSide)
                                            }
                                        },
                                        keyboardOptions = keyboardOptionsNumber)
                                }

                                // Description text
                                Text(
                                    text = stringResource(R.string.config_render_texture_description),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                        }
                    }

                    item {
                        Column(modifier = Modifier.padding(start = 8.dp, end = 8.dp),
                            verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            Text(text = stringResource(R.string.config_anti_aliasing_title))
                            Row(modifier = modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(6.dp),
                                verticalAlignment = Alignment.CenterVertically) {
                                val radioModifier = remember {
                                    modifier
                                        .height(32.dp)
                                        .weight(1f)
                                }

                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_anti_aliasing_follow_game), selected = config.value.renderTextureAntiAliasing == 0,
                                    onClick = { context?.onRenderTextureAntiAliasingChanged(0) })
                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_anti_aliasing_1x), selected = config.value.renderTextureAntiAliasing == 1,
                                    onClick = { context?.onRenderTextureAntiAliasingChanged(1) })
                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_anti_aliasing_2x), selected = config.value.renderTextureAntiAliasing == 2,
                                    onClick = { context?.onRenderTextureAntiAliasingChanged(2) })
                            }
                            Row(modifier = modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.spacedBy(6.dp),
                                verticalAlignment = Alignment.CenterVertically) {
                                val radioModifier = remember {
                                    modifier
                                        .height(32.dp)
                                        .weight(1f)
                                }

                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_anti_aliasing_4x), selected = config.value.renderTextureAntiAliasing == 4,
                                    onClick = { context?.onRenderTextureAntiAliasingChanged(4) })
                                GakuRadio(modifier = radioModifier,
                                    text = stringResource(R.string.config_anti_aliasing_8x), selected = config.value.renderTextureAntiAliasing == 8,
                                    onClick = { context?.onRenderTextureAntiAliasingChanged(8) })
                                Spacer(modifier = modifier.weight(1f))
                            }
                            
                            Text(
                                text = stringResource(R.string.config_anti_aliasing_description),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }

//                    item {
//                        HorizontalDivider(
//                            thickness = 1.dp,
//                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
//                        )
//                    }
//
//                    item {
//                        GakuSwitch(modifier.padding(start = 8.dp, end = 8.dp),
//                            stringResource(R.string.useCustomeGraphicSettings),
//                            checked = config.value.useCustomeGraphicSettings) {
//                                v -> context?.onUseCustomeGraphicSettingsChanged(v)
//                        }
//
//                        CollapsibleBox(modifier = modifier,
//                            expandState = config.value.useCustomeGraphicSettings,
//                            collapsedHeight = 0.dp,
//                            showExpand = false
//                        ) {
//                            LazyColumn(modifier = modifier
//                                .padding(8.dp)
//                                .sizeIn(maxHeight = screenH)
//                                .fillMaxWidth(),
//                                verticalArrangement = Arrangement.spacedBy(12.dp)
//                            ) {
//                                item {
//                                    Row(modifier = modifier.fillMaxWidth(),
//                                        horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                        val buttonModifier = remember {
//                                            modifier
//                                                .height(40.dp)
//                                                .weight(1f)
//                                        }
//
//                                        GakuButton(modifier = buttonModifier,
//                                            text = stringResource(R.string.max_high), onClick = { context?.onChangePresetQuality(4) })
//
//                                        GakuButton(modifier = buttonModifier,
//                                            text = stringResource(R.string.very_high), onClick = { context?.onChangePresetQuality(3) })
//
//                                        GakuButton(modifier = buttonModifier,
//                                            text = stringResource(R.string.hign), onClick = { context?.onChangePresetQuality(2) })
//
//                                        GakuButton(modifier = buttonModifier,
//                                            text = stringResource(R.string.middle), onClick = { context?.onChangePresetQuality(1) })
//
//                                        GakuButton(modifier = buttonModifier,
//                                            text = stringResource(R.string.low), onClick = { context?.onChangePresetQuality(0) })
//                                    }
//                                }
//
//                                item {
//                                    Row(modifier = modifier,
//                                        horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                        val textInputModifier = remember {
//                                            modifier
//                                                .height(45.dp)
//                                                .weight(1f)
//                                        }
//
//                                        GakuTextInput(modifier = textInputModifier,
//                                            fontSize = 14f,
//                                            value = config.value.renderScale.toString(),
//                                            onValueChange = { c -> context?.onRenderScaleChanged(c, 0, 0, 0)},
//                                            label = { Text(stringResource(R.string.renderscale)) },
//                                            keyboardOptions = keyBoardOptionsDecimal)
//
//                                        GakuTextInput(modifier = textInputModifier,
//                                            fontSize = 14f,
//                                            value = config.value.qualitySettingsLevel.toString(),
//                                            onValueChange = { c -> context?.onQualitySettingsLevelChanged(c, 0, 0, 0)},
//                                            label = { Text("QualityLevel (1/1/2/3/5)") },
//                                            keyboardOptions = keyboardOptionsNumber)
//                                    }
//                                }
//
//                                item {
//                                    Row(modifier = modifier,
//                                        horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                        val textInputModifier = remember {
//                                            modifier
//                                                .height(45.dp)
//                                                .weight(1f)
//                                        }
//
//                                        GakuTextInput(modifier = textInputModifier,
//                                            fontSize = 14f,
//                                            value = config.value.volumeIndex.toString(),
//                                            onValueChange = { c -> context?.onVolumeIndexChanged(c, 0, 0, 0)},
//                                            label = { Text("VolumeIndex (0/1/2/3/4)") },
//                                            keyboardOptions = keyboardOptionsNumber)
//
//                                        GakuTextInput(modifier = textInputModifier,
//                                            fontSize = 14f,
//                                            value = config.value.maxBufferPixel.toString(),
//                                            onValueChange = { c -> context?.onMaxBufferPixelChanged(c, 0, 0, 0)},
//                                            label = { Text("MaxBufferPixel (1024/1440/2538/3384/8190)", fontSize = 10.sp) },
//                                            keyboardOptions = keyboardOptionsNumber)
//                                    }
//                                }
//
//                                item {
//                                    Row(modifier = modifier,
//                                        horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                        val textInputModifier = remember {
//                                            modifier
//                                                .height(45.dp)
//                                                .weight(1f)
//                                        }
//
//                                        GakuTextInput(modifier = textInputModifier,
//                                            fontSize = 14f,
//                                            value = config.value.reflectionQualityLevel.toString(),
//                                            onValueChange = { c -> context?.onReflectionQualityLevelChanged(c, 0, 0, 0)},
//                                            label = { Text( text = "ReflectionLevel (0~5)") },
//                                            keyboardOptions = keyboardOptionsNumber)
//
//                                        GakuTextInput(modifier = textInputModifier,
//                                            fontSize = 14f,
//                                            value = config.value.lodQualityLevel.toString(),
//                                            onValueChange = { c -> context?.onLodQualityLevelChanged(c, 0, 0, 0)},
//                                            label = { Text("LOD Level (0~5)") },
//                                            keyboardOptions = keyboardOptionsNumber)
//                                    }
//                                }
//                            }
//                        }
//
//                    }

                }

            }
        }

        item {
            Spacer(modifier = modifier.height(bottomSpacerHeight))
        }
    }
}


@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO)
@Composable
fun HomePagePreview(modifier: Modifier = Modifier, data: LinkuraConfig = LinkuraConfig()) {
    HomePage(modifier, previewData = data)
}
