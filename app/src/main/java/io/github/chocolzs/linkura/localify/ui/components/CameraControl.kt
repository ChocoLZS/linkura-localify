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
        hasPermission = OverlayManager.hasOverlayPermission(context)
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
                                OverlayManager.requestOverlayPermission(context)
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
                            Log.d(TAG, "Toggle overlay button clicked, current state: $isOverlayEnabled")
                            try {
                                val newState = OverlayManager.toggleCameraOverlay(context)
                                isOverlayEnabled = newState
                                Log.d(TAG, "Overlay toggle completed, new state: $newState")
                            } catch (e: Exception) {
                                Log.e(TAG, "Error toggling overlay", e)
                            }
                        }
                    )
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
}