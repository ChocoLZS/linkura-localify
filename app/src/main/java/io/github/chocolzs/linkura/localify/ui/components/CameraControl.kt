package io.github.chocolzs.linkura.localify.ui.components

import android.util.Log
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.ui.overlay.OverlayManager

@Composable
fun CameraControl(
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    var isOverlayEnabled by remember { mutableStateOf(OverlayManager.isOverlayRunning()) }
    var hasPermission by remember { mutableStateOf(OverlayManager.hasOverlayPermission(context)) }
    
    val TAG = "CameraControl"

    // Check permission status on composition
    LaunchedEffect(Unit) {
        hasPermission = OverlayManager.hasOverlayPermission(context)
    }

    GakuGroupBox(
        title = stringResource(R.string.overlay_camera_info_title),
        modifier = modifier
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = stringResource(R.string.overlay_camera_info_description),
                style = MaterialTheme.typography.bodyMedium
            )

            if (!hasPermission) {
                Card(
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.errorContainer
                    )
                ) {
                    Column(
                        modifier = Modifier.padding(12.dp)
                    ) {
                        Text(
                            text = stringResource(R.string.overlay_camera_info_permission_required),
                            style = MaterialTheme.typography.labelLarge,
                            color = MaterialTheme.colorScheme.onErrorContainer
                        )
                        Text(
                            text = stringResource(R.string.overlay_camera_info_permission_description),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onErrorContainer
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        GakuButton(
                            text = stringResource(R.string.overlay_camera_info_grant_permission),
                            onClick = {
                                Log.d(TAG, "Grant Permission button clicked")
                                Log.d(TAG, "Context type: ${context.javaClass.simpleName}")
                                try {
                                    OverlayManager.requestOverlayPermission(context)
                                    Log.d(TAG, "Permission request completed")
                                } catch (e: Exception) {
                                    Log.e(TAG, "Error requesting permission", e)
                                }
                            }
                        )
                    }
                }
            } else {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = if (isOverlayEnabled) stringResource(R.string.overlay_camera_info_status_active) else stringResource(R.string.overlay_camera_info_status_inactive),
                        style = MaterialTheme.typography.bodyMedium
                    )
                    
                    GakuButton(
                        text = if (isOverlayEnabled) stringResource(R.string.overlay_camera_info_stop) else stringResource(R.string.overlay_camera_info_start),
                        onClick = {
                            try {
                                val newState = OverlayManager.toggleCameraOverlay(context)
                                
                                isOverlayEnabled = newState
                            } catch (e: Exception) {
                                Log.e(TAG, "=== EXCEPTION IN OVERLAY TOGGLE ===", e)
                                Log.e(TAG, "Exception type: ${e.javaClass.simpleName}")
                                Log.e(TAG, "Exception message: ${e.message}")
                                Log.e(TAG, "Exception cause: ${e.cause}")
                                Log.e(TAG, "Stack trace: ${e.stackTrace.contentToString()}")
                            }
                        }
                    )
                }
            }
                
                if (isOverlayEnabled) {
                    Card(
                        colors = CardDefaults.cardColors(
                            containerColor = MaterialTheme.colorScheme.primaryContainer
                        )
                    ) {
                        Text(
                            text = stringResource(R.string.overlay_camera_info_running_description),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                            modifier = Modifier.padding(12.dp)
                        )
                    }
                }
            }
        }
    }
