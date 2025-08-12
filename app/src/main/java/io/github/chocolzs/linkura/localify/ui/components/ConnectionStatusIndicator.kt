package io.github.chocolzs.linkura.localify.ui.components

import androidx.compose.animation.core.*
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import io.github.chocolzs.linkura.localify.R
import android.app.ActivityManager
import android.content.Context
import io.github.chocolzs.linkura.localify.ipc.LinkuraAidlService

@Composable
fun ConnectionStatusIndicator(
    modifier: Modifier = Modifier,
    size: Float = 16f,
    showDialog: Boolean = false,
    onDismissDialog: () -> Unit = {},
    onShowDialog: () -> Unit = {}
) {
    val context = LocalContext.current
    var isConnected by remember { mutableStateOf(false) }
    
    // Update connection status - check if AIDL service is running AND has clients
    LaunchedEffect(Unit) {
        while (true) {
            isConnected = isAidlServiceConnected(context)
            kotlinx.coroutines.delay(1000) // Update every second
        }
    }
    
    // Animation for radar effect when connected
    val infiniteTransition = rememberInfiniteTransition(label = "radar")
    val radarScale by infiniteTransition.animateFloat(
        initialValue = 0.8f,
        targetValue = 1.5f,
        animationSpec = infiniteRepeatable(
            animation = tween(2000, easing = LinearEasing),
            repeatMode = RepeatMode.Restart
        ),
        label = "radarScale"
    )
    
    val radarAlpha by infiniteTransition.animateFloat(
        initialValue = 0.8f,
        targetValue = 0.0f,
        animationSpec = infiniteRepeatable(
            animation = tween(2000, easing = LinearEasing),
            repeatMode = RepeatMode.Restart
        ),
        label = "radarAlpha"
    )
    
    Box(
        modifier = modifier
            .size(size.dp)
            .clickable { onShowDialog() },
        contentAlignment = Alignment.Center
    ) {
        // Radar effect for connected state
        if (isConnected) {
            Canvas(
                modifier = Modifier.size((size * radarScale).dp)
            ) {
                drawRadarEffect(
                    color = Color(0xFF4CAF50),
                    alpha = radarAlpha,
                    scale = radarScale
                )
            }
        }
        
        // Main status dot
        Box(
            modifier = Modifier
                .size((size * 0.6f).dp)
                .clip(CircleShape)
                .background(
                    if (isConnected) Color(0xFF4CAF50) else Color(0xFF9E9E9E)
                )
        )
    }
    
    // Info Dialog
    if (showDialog) {
        ConnectionStatusDialog(
            isConnected = isConnected,
            onDismiss = onDismissDialog
        )
    }
}

@Composable
private fun ConnectionStatusDialog(
    isConnected: Boolean,
    onDismiss: () -> Unit
) {
    Dialog(onDismissRequest = onDismiss) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            shape = MaterialTheme.shapes.large
        ) {
            Column(
                modifier = Modifier.padding(20.dp),
                verticalArrangement = Arrangement.spacedBy(16.dp)
            ) {
                // Title
                Text(
                    text = stringResource(R.string.connection_status_dialog_title),
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
                
                // Status indicator
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.Center,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    ConnectionStatusIndicator(
                        size = 24f,
                        showDialog = false
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = if (isConnected) 
                            stringResource(R.string.connection_status_connected) 
                        else 
                            stringResource(R.string.connection_status_disconnected),
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Medium
                    )
                }
                
                // Description
                Text(
                    text = stringResource(R.string.connection_status_dialog_description),
                    fontSize = 14.sp,
                    lineHeight = 20.sp
                )
                
                // Features list
                Text(
                    text = stringResource(R.string.connection_status_features_title),
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Medium
                )
                
                Column(
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    Text(
                        text = stringResource(R.string.connection_status_feature_1),
                        fontSize = 14.sp,
                        lineHeight = 18.sp
                    )
                    Text(
                        text = stringResource(R.string.connection_status_feature_2),
                        fontSize = 14.sp,
                        lineHeight = 18.sp
                    )
                    Text(
                        text = stringResource(R.string.connection_status_feature_3),
                        fontSize = 14.sp,
                        lineHeight = 18.sp
                    )
                }
                
                // Troubleshooting note
                if (!isConnected) {
                    HorizontalDivider()
                    Text(
                        text = stringResource(R.string.connection_status_troubleshooting),
                        fontSize = 14.sp,
                        lineHeight = 18.sp,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                
                // Close button
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.End
                ) {
                    TextButton(onClick = onDismiss) {
                        Text(stringResource(R.string.ok))
                    }
                }
            }
        }
    }
}

private fun DrawScope.drawRadarEffect(
    color: Color,
    alpha: Float,
    scale: Float
) {
    val radius = size.minDimension / 2 * scale
    val center = center
    
    drawCircle(
        color = color,
        radius = radius,
        center = center,
        alpha = alpha
    )
}

/**
 * Check if LinkuraAidlService is currently running AND has connected clients
 */
private fun isAidlServiceConnected(context: Context): Boolean {
    // First check if service is running
    val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
    val runningServices = activityManager.getRunningServices(Integer.MAX_VALUE)
    
    var serviceRunning = false
    for (serviceInfo in runningServices) {
        if (serviceInfo.service.className == LinkuraAidlService::class.java.name) {
            serviceRunning = true
            break
        }
    }
    
    if (!serviceRunning) {
        return false
    }
    
    // Then check if service has connected clients
    return try {
        val serviceInstance = LinkuraAidlService.getInstance()
        val clientCount = serviceInstance?.binder?.clientCount ?: 0
        clientCount > 0
    } catch (e: Exception) {
        false
    }
}