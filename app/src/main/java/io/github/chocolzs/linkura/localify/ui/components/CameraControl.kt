package io.github.chocolzs.linkura.localify.ui.components

import android.util.Log
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
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
        Log.d(TAG, "CameraControl LaunchedEffect: checking permissions")
        hasPermission = OverlayManager.hasOverlayPermission(context)
        Log.d(TAG, "CameraControl initial permission state: $hasPermission")
        Log.d(TAG, "CameraControl initial overlay state: $isOverlayEnabled")
    }

    GakuGroupBox(
        title = "Camera Info Overlay",
        modifier = modifier
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(
                text = "Display camera parameters in a floating window",
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
                            text = "Permission Required",
                            style = MaterialTheme.typography.labelLarge,
                            color = MaterialTheme.colorScheme.onErrorContainer
                        )
                        Text(
                            text = "The app needs permission to display overlays on other apps.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onErrorContainer
                        )
                        Spacer(modifier = Modifier.height(8.dp))
                        GakuButton(
                            text = "Grant Permission",
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
                        text = if (isOverlayEnabled) "Overlay Active" else "Overlay Inactive",
                        style = MaterialTheme.typography.bodyMedium
                    )
                    
                    GakuButton(
                        text = if (isOverlayEnabled) "Stop Overlay" else "Start Overlay",
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
                            text = "Camera overlay is running. The floating window shows real-time camera parameters including position, rotation, FOV, mode, and scene type.",
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                            modifier = Modifier.padding(12.dp)
                        )
                    }
                }
            }
        }
    }
