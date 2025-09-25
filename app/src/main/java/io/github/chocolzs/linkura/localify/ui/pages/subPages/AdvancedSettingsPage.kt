package io.github.chocolzs.linkura.localify.ui.pages.subPages

import io.github.chocolzs.linkura.localify.ui.components.GakuGroupBox
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import io.github.chocolzs.linkura.localify.MainActivity
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.getConfigState
import io.github.chocolzs.linkura.localify.models.ToolbarCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.ToolbarCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.FirstPersonCameraCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.FirstPersonCameraCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.CameraSensitivityCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.CameraSensitivityCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.StorySettingsCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.StorySettingsCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.CameraSettingsCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.CameraSettingsCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.ui.components.base.CollapsibleBox
import io.github.chocolzs.linkura.localify.ui.components.GakuSwitch
import io.github.chocolzs.linkura.localify.ui.components.GakuTextInput
import io.github.chocolzs.linkura.localify.ui.components.GakuSlider
import io.github.chocolzs.linkura.localify.ui.components.OverlayToolbarControl
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ElevatedButton
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import io.github.chocolzs.linkura.localify.models.LiveStreamCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.LiveStreamCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.utils.CameraSensitivityState
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

@Composable
fun SensitivityControl(
    modifier: Modifier = Modifier,
    text: String,
    value: Float,
    onValueChange: (Float) -> Unit,
    step: Float = 0.01f,
    minValue: Float = 0.1f,
    maxValue: Float = 5.0f
) {
    var isLongPressing by remember { mutableStateOf(false) }
    val coroutineScope = rememberCoroutineScope()
    
    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        Text(
            text = text,
            fontSize = 14.sp,
            color = MaterialTheme.colorScheme.onSurface
        )
        
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            // Decrease button (square shape)
            ElevatedButton(
                onClick = {
                    val newValue = (value - step).coerceIn(minValue, maxValue)
                    onValueChange(newValue)
                },
                modifier = Modifier
                    .size(48.dp)
                    .pointerInput(Unit) {
                        detectTapGestures(
                            onPress = {
                                isLongPressing = true
                                coroutineScope.launch {
                                    delay(500) // Initial delay
                                    while (isLongPressing) {
                                        val newValue = (value - step).coerceIn(minValue, maxValue)
                                        onValueChange(newValue)
                                        delay(100) // Repeat delay
                                    }
                                }
                                tryAwaitRelease()
                                isLongPressing = false
                            }
                        )
                    },
                colors = ButtonDefaults.elevatedButtonColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                    contentColor = MaterialTheme.colorScheme.onSurface
                ),
                shape = RoundedCornerShape(4.dp),
                elevation = ButtonDefaults.elevatedButtonElevation(defaultElevation = 2.dp),
                contentPadding = PaddingValues(0.dp)
            ) {
                Text(
                    text = "−",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center
                )
            }
            
            // Value input field
            GakuTextInput(
                modifier = Modifier
                    .weight(1f)
                    .height(48.dp),
                value = String.format("%.2f", value),
                onValueChange = { newText ->
                    try {
                        val newValue = newText.toFloat().coerceIn(minValue, maxValue)
                        onValueChange(newValue)
                    } catch (e: NumberFormatException) {
                        // Ignore invalid input
                    }
                },
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                fontSize = 14f
            )
            
            // Increase button (square shape)
            ElevatedButton(
                onClick = {
                    val newValue = (value + step).coerceIn(minValue, maxValue)
                    onValueChange(newValue)
                },
                modifier = Modifier
                    .size(48.dp)
                    .pointerInput(Unit) {
                        detectTapGestures(
                            onPress = {
                                isLongPressing = true
                                coroutineScope.launch {
                                    delay(500) // Initial delay
                                    while (isLongPressing) {
                                        val newValue = (value + step).coerceIn(minValue, maxValue)
                                        onValueChange(newValue)
                                        delay(100) // Repeat delay
                                    }
                                }
                                tryAwaitRelease()
                                isLongPressing = false
                            }
                        )
                    },
                colors = ButtonDefaults.elevatedButtonColors(
                    containerColor = MaterialTheme.colorScheme.surface,
                    contentColor = MaterialTheme.colorScheme.onSurface
                ),
                shape = RoundedCornerShape(4.dp),
                elevation = ButtonDefaults.elevatedButtonElevation(defaultElevation = 2.dp),
                contentPadding = PaddingValues(0.dp)
            ) {
                Text(
                    text = "+",
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center
                )
            }
        }
    }
}

@Composable
fun AdvanceSettingsPage(modifier: Modifier = Modifier,
             context: MainActivity? = null,
             previewData: LinkuraConfig? = null,
             bottomSpacerHeight: Dp = 120.dp,
             screenH: Dp = 1080.dp) {
    val config = getConfigState(context, previewData)
    // val scrollState = rememberScrollState()

    val toolbarViewModel: ToolbarCollapsibleBoxViewModel =
        viewModel(factory = ToolbarCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    val firstPersonCameraViewModel: FirstPersonCameraCollapsibleBoxViewModel =
        viewModel(factory = FirstPersonCameraCollapsibleBoxViewModelFactory(initiallyExpanded = config.value.firstPersonCameraHideHead))
    val cameraSensitivityViewModel: CameraSensitivityCollapsibleBoxViewModel =
        viewModel(factory = CameraSensitivityCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    val storySettingsViewModel: StorySettingsCollapsibleBoxViewModel =
        viewModel(factory = StorySettingsCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    val cameraSettingsViewModel: CameraSettingsCollapsibleBoxViewModel =
        viewModel(factory = CameraSettingsCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    val liveStreamViewModel: LiveStreamCollapsibleBoxViewModel =
        viewModel(factory = LiveStreamCollapsibleBoxViewModelFactory(initiallyExpanded = false))

    val keyBoardOptionsDecimal = remember {
        KeyboardOptions(keyboardType = KeyboardType.Decimal)
    }

    LazyColumn(modifier = modifier
        .sizeIn(maxHeight = screenH)
        // .fillMaxHeight()
        // .verticalScroll(scrollState)
        .fillMaxWidth(),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Floating Window Section (moved to top)
        item {
            GakuGroupBox(
                title = stringResource(R.string.overlay_toolbar_title),
                modifier = modifier,
                onHeadClick = {
                    toolbarViewModel.expanded = !toolbarViewModel.expanded
                }
            ) {
                CollapsibleBox(
                    modifier = modifier,
                    expandState = toolbarViewModel.expanded,
                    collapsedHeight = 0.dp,
                    showExpand = false
                ) {
                    OverlayToolbarControl(modifier = modifier)
                    GakuSwitch(modifier, stringResource(R.string.overlay_toolbar_in_game_toolbar_title), checked = config.value.enableInGameOverlayToolbar) {
                            v -> context?.onEnableInGameOverlayToolbar(v)
                    }
                }
            }
            Spacer(Modifier.height(6.dp))
        }

        item {
            GakuGroupBox(
                modifier, 
                stringResource(R.string.camera_settings),
                onHeadClick = {
                    cameraSettingsViewModel.expanded = !cameraSettingsViewModel.expanded
                }
            ) {
                CollapsibleBox(
                    modifier = modifier,
                    expandState = cameraSettingsViewModel.expanded,
                    collapsedHeight = 0.dp,
                    showExpand = false
                ) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.enable_free_camera), checked = config.value.enableFreeCamera) {
                            v -> context?.onEnableFreeCameraChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.config_character_render_head_only_title), checked = config.value.hideCharacterBody) {
                            v -> context?.onHideCharacterBodyChanged(v)
                    }
                    CollapsibleBox(
                        modifier = modifier,
                        expandState = config.value.hideCharacterBody,
                        collapsedHeight = 0.dp,
                        showExpand = false
                    ) {
                        Text(
                            text = stringResource(R.string.config_character_render_head_only_description),
                            fontSize = 12.sp,
                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                            modifier = modifier.padding(start = 16.dp, top = 4.dp, bottom = 8.dp)
                        )
                    }
                    
                    HorizontalDivider(
                        thickness = 1.dp,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                    )
                    
                    // First Person Camera Settings
                    GakuSwitch(
                        modifier,
                        stringResource(R.string.config_first_person_camera_hide_head), 
                        checked = config.value.firstPersonCameraHideHead
                    ) { v -> 
                        context?.onFirstPersonCameraHideHeadChanged(v)
                        firstPersonCameraViewModel.expanded = v
                    }
                    
                    // Note text for head accessories
                    Text(
                        text = stringResource(R.string.config_first_person_camera_head_accessories_note),
                        fontSize = 12.sp,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f),
                        modifier = modifier.padding(start = 4.dp, top = 2.dp, bottom = 4.dp)
                    )
                    
                    // Sub-panel that expands when main switch is on
                    CollapsibleBox(
                        modifier = modifier,
                        expandState = config.value.firstPersonCameraHideHead,
                        collapsedHeight = 0.dp,
                        showExpand = false
                    ) {
                        Column(
                            modifier = modifier.padding(start = 16.dp, top = 4.dp),
                            verticalArrangement = Arrangement.spacedBy(4.dp)
                        ) {
                            GakuSwitch(
                                modifier, 
                                stringResource(R.string.config_first_person_camera_hide_hair), 
                                checked = config.value.firstPersonCameraHideHair
                            ) { v -> context?.onFirstPersonCameraHideHairChanged(v) }
                            
                            GakuSwitch(
                                modifier, 
                                stringResource(R.string.config_first_person_camera_hide_face), 
                                checked = true,
                                enabled = false
                            ) { _ -> /* No-op since this is disabled */ }
                        }
                    }
                    
                    }
                }
            }

            Spacer(Modifier.height(6.dp))
        }

        // Camera Sensitivity Settings Section
        item {
            GakuGroupBox(
                modifier, 
                stringResource(R.string.config_camera_sensitivity_title),
                onHeadClick = {
                    cameraSensitivityViewModel.expanded = !cameraSensitivityViewModel.expanded
                }
            ) {
                CollapsibleBox(
                    modifier = modifier,
                    expandState = cameraSensitivityViewModel.expanded,
                    collapsedHeight = 0.dp,
                    showExpand = false
                ) {
                    Column(
                        modifier = modifier.padding(top = 8.dp, bottom = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        SensitivityControl(
                            text = stringResource(R.string.config_camera_sensitivity_movement),
                            value = CameraSensitivityState.movementSensitivity,
                            onValueChange = { value -> 
                                context?.onCameraMovementSensitivityChanged(value)
                                context?.let { ctx -> CameraSensitivityState.updateMovementSensitivity(ctx, value) }
                            }
                        )
                        
                        SensitivityControl(
                            text = stringResource(R.string.config_camera_sensitivity_vertical),
                            value = CameraSensitivityState.verticalSensitivity,
                            onValueChange = { value -> 
                                context?.onCameraVerticalSensitivityChanged(value)
                                context?.let { ctx -> CameraSensitivityState.updateVerticalSensitivity(ctx, value) }
                            }
                        )
                        
                        SensitivityControl(
                            text = stringResource(R.string.config_camera_sensitivity_fov),
                            value = CameraSensitivityState.fovSensitivity,
                            onValueChange = { value -> 
                                context?.onCameraFovSensitivityChanged(value)
                                context?.let { ctx -> CameraSensitivityState.updateFovSensitivity(ctx, value) }
                            }
                        )
                        
                        SensitivityControl(
                            text = stringResource(R.string.config_camera_sensitivity_rotation),
                            value = CameraSensitivityState.rotationSensitivity,
                            onValueChange = { value -> 
                                context?.onCameraRotationSensitivityChanged(value)
                                context?.let { ctx -> CameraSensitivityState.updateRotationSensitivity(ctx, value) }
                            }
                        )
                    }
                }
            }

            Spacer(Modifier.height(6.dp))
        }
        item {
            GakuGroupBox(
                modifier,
                stringResource(R.string.config_motion_capture_title),
                onHeadClick = {
                    liveStreamViewModel.expanded = !liveStreamViewModel.expanded
                }
            ) {
                CollapsibleBox(
                    modifier = modifier,
                    expandState = liveStreamViewModel.expanded,
                    collapsedHeight = 0.dp,
                    showExpand = false
                ) {
                    GakuSwitch(modifier, stringResource(R.string.config_motion_capture_hide_character_foot_shadow), checked = config.value.hideCharacterShadow) {
                            v -> context?.onHideCharacterShadowChanged(v)
                    }
                    GakuSlider(
                        modifier = modifier,
                        text = stringResource(R.string.config_motion_capture_hide_scene_items_level),
                        value = config.value.hideLiveStreamSceneItemsLevel.toFloat(),
                        valueRange = 0.0f..4.0f,
                        markerValues = listOf(0.0f, 1.0f, 2.0f, 3.0f, 4.0f),
                        steps = 3,
                        onValueChange = { v -> context?.onHideLiveStreamSceneItemsLevel(v.toInt()) }
                    )
                    Text(
                        text = when (config.value.hideLiveStreamSceneItemsLevel) {
                            0 -> stringResource(R.string.config_motion_capture_hide_scene_items_level_0_description)
                            1 -> stringResource(R.string.config_motion_capture_hide_scene_items_level_1_description)
                            2 -> stringResource(R.string.config_motion_capture_hide_scene_items_level_2_description)
                            3 -> stringResource(R.string.config_motion_capture_hide_scene_items_level_3_description)
                            4 -> stringResource(R.string.config_motion_capture_hide_scene_items_level_4_description)
                            else -> ""
                        },
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                    )
                    GakuSwitch(modifier, stringResource(R.string.config_motion_capture_hide_character_items), checked = config.value.hideLiveStreamCharacterItems) {
                            v -> context?.onHideLiveStreamCharacterItems(v)
                    }
                    Text(
                        text = stringResource(R.string.config_motion_capture_hide_character_items_description),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                    )
                    GakuSwitch(modifier, stringResource(R.string.config_motion_capture_archive_fes_unlock_ticket), checked = config.value.fesArchiveUnlockTicket) {
                            v -> context?.onFesArchiveUnlockTicketChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.config_motion_capture_after_unlock), checked = config.value.unlockAfter) {
                            v -> context?.onUnlockAfterChanged(v)
                    }
                    GakuSwitch(modifier, "cover", checked = config.value.removeRenderImageCover) {
                            v -> context?.onRemoveRenderImageCoverChanged(v)
                    }
                    GakuSwitch(modifier, "character", checked = config.value.avoidCharacterExit) {
                            v -> context?.onAvoidCharacterExitChanged(v)
                    }
                }
            }

            Spacer(Modifier.height(6.dp))
        }
        item {
            GakuGroupBox(
                modifier, 
                stringResource(R.string.config_story_settings_title),
                onHeadClick = {
                    storySettingsViewModel.expanded = !storySettingsViewModel.expanded
                }
            ) {
                CollapsibleBox(
                    modifier = modifier,
                    expandState = storySettingsViewModel.expanded,
                    collapsedHeight = 0.dp,
                    showExpand = false
                ) {
                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.config_story_hide_background), checked = config.value.storyHideBackground) {
                            v -> context?.onStoryHideBackgroundChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.config_story_hide_transition), checked = config.value.storyHideTransition) {
                            v -> context?.onStoryHideTransitionChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.config_story_hide_non_character_3d), checked = config.value.storyHideNonCharacter3d) {
                            v -> context?.onStoryHideNonCharacter3dChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.config_story_hide_dof), checked = config.value.storyHideDof) {
                            v -> context?.onStoryHideDofChanged(v)
                    }
                    GakuSwitch(modifier, stringResource(R.string.config_story_hide_effect), checked = config.value.storyHideEffect) {
                            v -> context?.onStoryHideEffectChanged(v)
                    }
                    
                    HorizontalDivider(
                        thickness = 1.dp,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
                    )
                    
                    GakuSlider(
                        modifier = modifier,
                        text = stringResource(R.string.config_story_vocal_text_duration_rate),
                        value = config.value.storyNovelVocalTextDurationRate,
                        valueRange = 0.5f..10.0f,
                        onValueChange = { v -> context?.onStoryNovelVocalTextDurationRateChanged(v) }
                    )
                    
                    GakuSlider(
                        modifier = modifier,
                        text = stringResource(R.string.config_story_non_vocal_text_duration_rate),
                        value = config.value.storyNovelNonVocalTextDurationRate,
                        valueRange = 0.5f..10.0f,
                        onValueChange = { v -> context?.onStoryNovelNonVocalTextDurationRateChanged(v) }
                    )
                    }
                }
            }

            Spacer(Modifier.height(6.dp))
        }

        item {
            GakuGroupBox(modifier, stringResource(R.string.debug_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
//                    GakuSwitch(modifier, stringResource(R.string.useMasterDBTrans), checked = config.value.useMasterTrans) {
//                            v -> context?.onUseMasterTransChanged(v)
//                    }

                    GakuSwitch(modifier, stringResource(R.string.text_hook_test_mode), checked = config.value.textTest) {
                            v -> context?.onTextTestChanged(v)
                    }

//                    GakuSwitch(modifier, stringResource(R.string.export_text), checked = config.value.dumpText) {
//                            v -> context?.onDumpTextChanged(v)
//                    }

                    GakuSwitch(modifier, stringResource(R.string.force_export_resource), checked = config.value.forceExportResource) {
                            v -> context?.onForceExportResourceChanged(v)
                    }
                    Text(
                        text = "用于开发时实时动态更新资源",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.7f)
                    )

//                    GakuSwitch(modifier, stringResource(R.string.login_as_ios), checked = config.value.loginAsIOS) {
//                            v -> context?.onLoginAsIOSChanged(v)
//                    }
                }
            }

            Spacer(Modifier.height(6.dp))
        }

        item {
            Spacer(modifier = modifier.height(bottomSpacerHeight))
        }
    }
}


@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO)
@Composable
fun AdvanceSettingsPagePreview(modifier: Modifier = Modifier, data: LinkuraConfig = LinkuraConfig()) {
    AdvanceSettingsPage(modifier, previewData = data)
}
