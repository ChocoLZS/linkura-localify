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
fun OverlayToolbarControl(
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    var isOverlayEnabled by remember { mutableStateOf(OverlayManager.isOverlayRunning()) }
    var hasPermission by remember { mutableStateOf(OverlayManager.hasOverlayPermission(context)) }
    
    val TAG = "OverlayToolbarControl"

    // Check permission status on composition
    LaunchedEffect(Unit) {
        hasPermission = OverlayManager.hasOverlayPermission(context)
    }
    
    // Monitor overlay state changes
    LaunchedEffect(isOverlayEnabled) {
        // Periodically check overlay state in case it was stopped externally
        while (true) {
            kotlinx.coroutines.delay(1000) // Check every second
            val currentState = OverlayManager.isOverlayRunning()
            if (currentState != isOverlayEnabled) {
                isOverlayEnabled = currentState
            }
        }
    }

    GakuGroupBox(
        title = stringResource(R.string.overlay_toolbar_title),
        modifier = modifier
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = stringResource(R.string.overlay_toolbar_description),
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
                            text = stringResource(R.string.overlay_toolbar_permission_required),
                            style = MaterialTheme.typography.labelLarge,
                            color = MaterialTheme.colorScheme.onErrorContainer
                        )
                        Text(
                            text = stringResource(R.string.overlay_toolbar_permission_description),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onErrorContainer
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        GakuButton(
                            text = stringResource(R.string.overlay_toolbar_grant_permission),
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
                        text = if (isOverlayEnabled) stringResource(R.string.overlay_toolbar_status_active) else stringResource(R.string.overlay_toolbar_status_inactive),
                        style = MaterialTheme.typography.bodyMedium
                    )
                    
                    GakuButton(
                        text = if (isOverlayEnabled) stringResource(R.string.overlay_toolbar_stop) else stringResource(R.string.overlay_toolbar_start),
                        onClick = {
                            try {
                                val newState = OverlayManager.toggleOverlay(context)
                                
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
                        Column(
                            modifier = Modifier.padding(12.dp)
                        ) {
                            Text(
                                text = stringResource(R.string.overlay_toolbar_crash_warning),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onPrimaryContainer
                            )
                        }
                    }
                }
            }
        }
    }
