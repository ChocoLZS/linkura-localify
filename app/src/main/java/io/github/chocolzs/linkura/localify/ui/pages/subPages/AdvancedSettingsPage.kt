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
import io.github.chocolzs.linkura.localify.models.BreastCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.BreastCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.FirstPersonCameraCollapsibleBoxViewModel
import io.github.chocolzs.linkura.localify.models.FirstPersonCameraCollapsibleBoxViewModelFactory
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.ui.components.base.CollapsibleBox
import io.github.chocolzs.linkura.localify.ui.components.GakuButton
import io.github.chocolzs.linkura.localify.ui.components.GakuSwitch
import io.github.chocolzs.linkura.localify.ui.components.GakuTextInput
import io.github.chocolzs.linkura.localify.ui.components.GakuSlider
import io.github.chocolzs.linkura.localify.ui.components.CameraControl


@Composable
fun AdvanceSettingsPage(modifier: Modifier = Modifier,
             context: MainActivity? = null,
             previewData: LinkuraConfig? = null,
             bottomSpacerHeight: Dp = 120.dp,
             screenH: Dp = 1080.dp) {
    val config = getConfigState(context, previewData)
    // val scrollState = rememberScrollState()

    val breastParamViewModel: BreastCollapsibleBoxViewModel =
        viewModel(factory = BreastCollapsibleBoxViewModelFactory(initiallyExpanded = false))
    val firstPersonCameraViewModel: FirstPersonCameraCollapsibleBoxViewModel =
        viewModel(factory = FirstPersonCameraCollapsibleBoxViewModelFactory(initiallyExpanded = config.value.firstPersonCameraHideHead))
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
        item {
            GakuGroupBox(modifier, stringResource(R.string.camera_settings)) {
                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                    GakuSwitch(modifier, stringResource(R.string.enable_free_camera), checked = config.value.enableFreeCamera) {
                            v -> context?.onEnableFreeCameraChanged(v)
                    }
                    GakuSwitch(modifier, "cover", checked = config.value.removeRenderImageCover) {
                            v -> context?.onRemoveRenderImageCoverChanged(v)
                    }
                    GakuSwitch(modifier, "character", checked = config.value.avoidCharacterExit) {
                            v -> context?.onAvoidCharacterExitChanged(v)
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

            Spacer(Modifier.height(6.dp))
        }

        item {
            CameraControl(modifier = modifier)
            Spacer(Modifier.height(6.dp))
        }

        item {
            GakuGroupBox(modifier, stringResource(R.string.config_story_settings_title)) {
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

            Spacer(Modifier.height(6.dp))
        }

//        item {
//            GakuGroupBox(modifier, stringResource(R.string.debug_settings)) {
//                Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
//                    GakuSwitch(modifier, stringResource(R.string.useMasterDBTrans), checked = config.value.useMasterTrans) {
//                            v -> context?.onUseMasterTransChanged(v)
//                    }
//
//                    GakuSwitch(modifier, stringResource(R.string.text_hook_test_mode), checked = config.value.textTest) {
//                            v -> context?.onTextTestChanged(v)
//                    }
//
//                    GakuSwitch(modifier, stringResource(R.string.export_text), checked = config.value.dumpText) {
//                            v -> context?.onDumpTextChanged(v)
//                    }
//
//                    GakuSwitch(modifier, stringResource(R.string.force_export_resource), checked = config.value.forceExportResource) {
//                            v -> context?.onForceExportResourceChanged(v)
//                    }
//
//                    GakuSwitch(modifier, stringResource(R.string.login_as_ios), checked = config.value.loginAsIOS) {
//                            v -> context?.onLoginAsIOSChanged(v)
//                    }
//                }
//            }
//
//            Spacer(Modifier.height(6.dp))
//        }
//
//        item {
//            GakuGroupBox(modifier, stringResource(R.string.breast_param),
//                contentPadding = 0.dp,
//                onHeadClick = {
//                    breastParamViewModel.expanded = !breastParamViewModel.expanded
//                }) {
//                CollapsibleBox(modifier = modifier,
//                    viewModel = breastParamViewModel
//                ) {
//                    LazyColumn(modifier = modifier
//                        .padding(8.dp)
//                        .sizeIn(maxHeight = screenH),
//                        verticalArrangement = Arrangement.spacedBy(12.dp)
//                    ) {
//                        item {
//                            GakuSwitch(modifier = modifier,
//                                checked = config.value.enableBreastParam,
//                                text = stringResource(R.string.enable_breast_param)
//                            ) { v -> context?.onEnableBreastParamChanged(v) }
//                        }
//                        item {
//                            Row(modifier = modifier.fillMaxWidth(),
//                                horizontalArrangement = Arrangement.spacedBy(2.dp)) {
//                                val buttonModifier = remember {
//                                    modifier
//                                        .height(40.dp)
//                                        .weight(1f)
//                                }
//
//                                GakuButton(modifier = buttonModifier,
//                                    text = "??", onClick = { context?.onBClickPresetChanged(5) })
//
//                                GakuButton(modifier = buttonModifier,
//                                    text = "+5", onClick = { context?.onBClickPresetChanged(4) })
//
//                                GakuButton(modifier = buttonModifier,
//                                    text = "+4", onClick = { context?.onBClickPresetChanged(3) })
//
//                                GakuButton(modifier = buttonModifier,
//                                    text = "+3", onClick = { context?.onBClickPresetChanged(2) })
//
//                                GakuButton(modifier = buttonModifier,
//                                    text = "+2", onClick = { context?.onBClickPresetChanged(1) })
//
//                                GakuButton(modifier = buttonModifier,
//                                    text = "+1", onClick = { context?.onBClickPresetChanged(0) })
//                            }
//                        }
//
//                        item {
//                            Row(modifier = modifier,
//                                horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                GakuTextInput(modifier = modifier
//                                    .height(45.dp)
//                                    .weight(1f),
//                                    fontSize = 14f,
//                                    value = config.value.bDamping.toString(),
//                                    onValueChange = { c -> context?.onBDampingChanged(c, 0, 0, 0)},
//                                    label = { Text(stringResource(R.string.damping)) },
//                                    keyboardOptions = keyBoardOptionsDecimal
//                                )
//
//                                GakuTextInput(modifier = modifier
//                                    .height(45.dp)
//                                    .weight(1f),
//                                    fontSize = 14f,
//                                    value = config.value.bStiffness.toString(),
//                                    onValueChange = { c -> context?.onBStiffnessChanged(c, 0, 0, 0)},
//                                    label = { Text(stringResource(R.string.stiffness)) },
//                                    keyboardOptions = keyBoardOptionsDecimal)
//                            }
//                        }
//
//                        item {
//                            Row(modifier = modifier,
//                                horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                GakuTextInput(modifier = modifier
//                                    .height(45.dp)
//                                    .weight(1f),
//                                    fontSize = 14f,
//                                    value = config.value.bSpring.toString(),
//                                    onValueChange = { c -> context?.onBSpringChanged(c, 0, 0, 0)},
//                                    label = { Text(stringResource(R.string.spring)) },
//                                    keyboardOptions = keyBoardOptionsDecimal
//                                )
//
//                                GakuTextInput(modifier = modifier
//                                    .height(45.dp)
//                                    .weight(1f),
//                                    fontSize = 14f,
//                                    value = config.value.bPendulum.toString(),
//                                    onValueChange = { c -> context?.onBPendulumChanged(c, 0, 0, 0)},
//                                    label = { Text(stringResource(R.string.pendulum)) },
//                                    keyboardOptions = keyBoardOptionsDecimal)
//                            }
//                        }
//
//                        item {
//                            Row(modifier = modifier,
//                                horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                GakuTextInput(modifier = modifier
//                                    .height(45.dp)
//                                    .weight(1f),
//                                    fontSize = 14f,
//                                    value = config.value.bPendulumRange.toString(),
//                                    onValueChange = { c -> context?.onBPendulumRangeChanged(c, 0, 0, 0)},
//                                    label = { Text(stringResource(R.string.pendulumrange)) },
//                                    keyboardOptions = keyBoardOptionsDecimal
//                                )
//
//                                GakuTextInput(modifier = modifier
//                                    .height(45.dp)
//                                    .weight(1f),
//                                    fontSize = 14f,
//                                    value = config.value.bAverage.toString(),
//                                    onValueChange = { c -> context?.onBAverageChanged(c, 0, 0, 0)},
//                                    label = { Text(stringResource(R.string.average)) },
//                                    keyboardOptions = keyBoardOptionsDecimal)
//                            }
//                        }
//
//                        item {
//                            GakuTextInput(modifier = modifier
//                                .height(45.dp)
//                                .fillMaxWidth(),
//                                fontSize = 14f,
//                                value = config.value.bRootWeight.toString(),
//                                onValueChange = { c -> context?.onBRootWeightChanged(c, 0, 0, 0)},
//                                label = { Text(stringResource(R.string.rootweight)) },
//                                keyboardOptions = keyBoardOptionsDecimal
//                            )
//                        }
//
//                        item {
//                            GakuSwitch(modifier = modifier,
//                                checked = config.value.bUseScale,
//                                leftPart = {
//                                    GakuTextInput(modifier = modifier
//                                        .height(45.dp),
//                                        fontSize = 14f,
//                                        value = config.value.bScale.toString(),
//                                        onValueChange = { c -> context?.onBScaleChanged(c, 0, 0, 0)},
//                                        label = { Text(stringResource(R.string.breast_scale)) },
//                                        keyboardOptions = keyBoardOptionsDecimal
//                                    )
//                                }
//                            ) { v -> context?.onBUseScaleChanged(v) }
//                        }
//
//                        item {
//                            GakuSwitch(modifier = modifier,
//                                checked = config.value.bUseArmCorrection,
//                                text = stringResource(R.string.usearmcorrection)
//                            ) { v -> context?.onBUseArmCorrectionChanged(v) }
//                        }
//
//                        item {
//                            HorizontalDivider(
//                                thickness = 1.dp,
//                                color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
//                            )
//                        }
//
//                        item {
//                            GakuSwitch(modifier = modifier,
//                                checked = config.value.bUseLimit,
//                                text = stringResource(R.string.uselimit_0_1)
//                            ) { v ->
//                                context?.onBUseLimitChanged(v)
//                            }
//                        }
//
//                        item {
//                            CollapsibleBox(modifier = modifier,
//                                expandState = config.value.bUseLimit,
//                                collapsedHeight = 0.dp,
//                                showExpand = false
//                            ){
//                                Row(modifier = modifier,
//                                    horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                    val textInputModifier = remember {
//                                        modifier
//                                            .height(45.dp)
//                                            .weight(1f)
//                                    }
//
//                                    GakuTextInput(modifier = textInputModifier,
//                                        fontSize = 14f,
//                                        value = config.value.bLimitXx.toString(),
//                                        onValueChange = { c -> context?.onBLimitXxChanged(c, 0, 0, 0)},
//                                        label = { Text(stringResource(R.string.axisx_x)) },
//                                        keyboardOptions = keyBoardOptionsDecimal
//                                    )
//
//                                    GakuTextInput(modifier = textInputModifier,
//                                        fontSize = 14f,
//                                        value = config.value.bLimitYx.toString(),
//                                        onValueChange = { c -> context?.onBLimitYxChanged(c, 0, 0, 0)},
//                                        label = { Text(stringResource(R.string.axisy_x)) },
//                                        keyboardOptions = keyBoardOptionsDecimal
//                                    )
//
//                                    GakuTextInput(modifier = textInputModifier,
//                                        fontSize = 14f,
//                                        value = config.value.bLimitZx.toString(),
//                                        onValueChange = { c -> context?.onBLimitZxChanged(c, 0, 0, 0)},
//                                        label = { Text(stringResource(R.string.axisz_x)) },
//                                        keyboardOptions = keyBoardOptionsDecimal
//                                    )
//                                }
//
//                                Row(modifier = modifier,
//                                    horizontalArrangement = Arrangement.spacedBy(4.dp)) {
//                                    val textInputModifier = remember {
//                                        modifier
//                                            .height(45.dp)
//                                            .weight(1f)
//                                    }
//
//                                    GakuTextInput(modifier = textInputModifier,
//                                        fontSize = 14f,
//                                        value = config.value.bLimitXy.toString(),
//                                        onValueChange = { c -> context?.onBLimitXyChanged(c, 0, 0, 0)},
//                                        label = { Text(stringResource(R.string.axisx_y)) },
//                                        keyboardOptions = keyBoardOptionsDecimal
//                                    )
//
//                                    GakuTextInput(modifier = textInputModifier,
//                                        fontSize = 14f,
//                                        value = config.value.bLimitYy.toString(),
//                                        onValueChange = { c -> context?.onBLimitYyChanged(c, 0, 0, 0)},
//                                        label = { Text(stringResource(R.string.axisy_y)) },
//                                        keyboardOptions = keyBoardOptionsDecimal
//                                    )
//
//                                    GakuTextInput(modifier = textInputModifier,
//                                        fontSize = 14f,
//                                        value = config.value.bLimitZy.toString(),
//                                        onValueChange = { c -> context?.onBLimitZyChanged(c, 0, 0, 0)},
//                                        label = { Text(stringResource(R.string.axisz_y)) },
//                                        keyboardOptions = keyBoardOptionsDecimal
//                                    )
//                                }
//                            }
//                        }
//
//                    }
//                }
//            }
//        }
//
//        item {
//            if (config.value.dbgMode) {
//                Spacer(Modifier.height(6.dp))
//
//                GakuGroupBox(modifier, stringResource(R.string.test_mode_live)) {
//                    Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
//                        GakuSwitch(modifier, stringResource(R.string.unlockAllLive),
//                            checked = config.value.unlockAllLive) {
//                                v -> context?.onUnlockAllLiveChanged(v)
//                        }
//                        GakuSwitch(modifier, stringResource(R.string.unlockAllLiveCostume),
//                            checked = config.value.unlockAllLiveCostume) {
//                                v -> context?.onUnlockAllLiveCostumeChanged(v)
//                        }
//
//                        /*
//                        HorizontalDivider(
//                            thickness = 1.dp,
//                            color = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.12f)
//                        )
//                        GakuSwitch(modifier, stringResource(R.string.liveUseCustomeDress),
//                            checked = config.value.enableLiveCustomeDress) {
//                                v -> context?.onLiveCustomeDressChanged(v)
//                        }
//                        GakuTextInput(modifier = modifier
//                            .height(45.dp)
//                            .fillMaxWidth(),
//                            fontSize = 14f,
//                            value = config.value.liveCustomeHeadId,
//                            onValueChange = { c -> context?.onLiveCustomeHeadIdChanged(c, 0, 0, 0)},
//                            label = { Text(stringResource(R.string.live_costume_head_id),
//                                fontSize = 12.sp) }
//                        )
//                        GakuTextInput(modifier = modifier
//                            .height(45.dp)
//                            .fillMaxWidth(),
//                            fontSize = 14f,
//                            value = config.value.liveCustomeCostumeId,
//                            onValueChange = { c -> context?.onLiveCustomeCostumeIdChanged(c, 0, 0, 0)},
//                            label = { Text(stringResource(R.string.live_custome_dress_id)) }
//                        )*/
//                    }
//                }
//            }
//        }

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
