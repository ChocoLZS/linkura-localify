package io.github.chocolzs.linkura.localify.ui.overlay

import android.graphics.PixelFormat
import android.os.Build
import android.util.Log
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import io.github.chocolzs.linkura.localify.ipc.MessageRouter
import io.github.chocolzs.linkura.localify.ipc.LinkuraMessages.*
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class CameraSensitivityOverlayService(private val parentService: OverlayService) {
    companion object {
        private const val TAG = "CameraSensitivityOverlayService"
    }
    
    private var overlayView: View? = null
    private var isVisible = false
    
    // Sensitivity values state
    private var movementSensitivity by mutableStateOf(1.0f)
    private var verticalSensitivity by mutableStateOf(1.0f)
    private var fovSensitivity by mutableStateOf(1.0f)
    private var rotationSensitivity by mutableStateOf(1.0f)
    
    // Touch handling for dragging
    private var initialX = 0
    private var initialY = 0
    private var initialTouchX = 0f
    private var initialTouchY = 0f
    
    fun show() {
        if (isVisible) return
        
        try {
            loadCurrentSensitivityValues()
            createOverlay()
            isVisible = true
        } catch (e: Exception) {
            Log.e(TAG, "Error showing camera sensitivity overlay", e)
        }
    }
    
    fun updateSensitivityValues(
        movement: Float,
        vertical: Float,
        fov: Float,
        rotation: Float
    ) {
        movementSensitivity = movement
        verticalSensitivity = vertical
        fovSensitivity = fov
        rotationSensitivity = rotation
        Log.d(TAG, "Updated sensitivity values: movement=$movementSensitivity, vertical=$verticalSensitivity, fov=$fovSensitivity, rotation=$rotationSensitivity")
    }
    
    private fun loadCurrentSensitivityValues() {
        try {
            val sharedPrefs = parentService.getSharedPreferences("camera_sensitivity", android.content.Context.MODE_PRIVATE)
            movementSensitivity = sharedPrefs.getFloat("movement", 1.0f)
            verticalSensitivity = sharedPrefs.getFloat("vertical", 1.0f)
            fovSensitivity = sharedPrefs.getFloat("fov", 1.0f)
            rotationSensitivity = sharedPrefs.getFloat("rotation", 1.0f)
            Log.d(TAG, "Loaded sensitivity values: movement=$movementSensitivity, vertical=$verticalSensitivity, fov=$fovSensitivity, rotation=$rotationSensitivity")
        } catch (e: Exception) {
            Log.e(TAG, "Error loading sensitivity values, using defaults", e)
            // Keep default values (1.0f for all)
        }
    }

    fun hide() {
        if (!isVisible) return
        
        try {
            overlayView?.let { view ->
                parentService.getWindowManagerInstance()?.removeView(view)
            }
            overlayView = null
            isVisible = false
            parentService.resetCameraSensitivityOverlayState()
        } catch (e: Exception) {
            Log.e(TAG, "Error hiding camera sensitivity overlay", e)
        }
    }

    private fun createOverlay() {
        val windowManager = parentService.getWindowManagerInstance()
        if (windowManager == null) {
            Log.e(TAG, "WindowManager is null, cannot create overlay")
            return
        }

        val layoutFlag = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
        } else {
            WindowManager.LayoutParams.TYPE_PHONE
        }

        val params = WindowManager.LayoutParams(
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            layoutFlag,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL or
                    WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT
        )

        params.gravity = Gravity.CENTER
        params.x = 0
        params.y = 0
        
        overlayView = ComposeView(parentService).apply {
            setViewTreeLifecycleOwner(parentService.getLifecycleOwnerInstance())
            setViewTreeSavedStateRegistryOwner(parentService.getSavedStateRegistryOwnerInstance())
            setContent {
                LocalifyTheme {
                    CameraSensitivityOverlay()
                }
            }
            
            // Add touch listener for dragging
            setOnTouchListener { v, event ->
                when (event.action) {
                    MotionEvent.ACTION_DOWN -> {
                        initialX = params.x
                        initialY = params.y
                        initialTouchX = event.rawX
                        initialTouchY = event.rawY
                        true
                    }
                    MotionEvent.ACTION_MOVE -> {
                        params.x = initialX + (event.rawX - initialTouchX).toInt()
                        params.y = initialY + (event.rawY - initialTouchY).toInt()
                        windowManager.updateViewLayout(v, params)
                        true
                    }
                    else -> false
                }
            }
        }
        windowManager.addView(overlayView, params)
    }

    @Composable
    private fun CameraSensitivityOverlay() {
        Box(
            modifier = Modifier
                .background(
                    Color.Black.copy(alpha = 0.8f),
                    RoundedCornerShape(12.dp)
                )
                .padding(16.dp)
                .width(280.dp)
        ) {
            Column(
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                // Title bar with close button
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Text(
                        text = parentService.getString(R.string.overlay_camera_sensitivity_floating_title),
                        color = Color.White,
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold
                    )
                    
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        // Reset button
                        IconButton(
                            onClick = { resetAllToDefault() },
                            modifier = Modifier.size(32.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.Refresh,
                                contentDescription = parentService.getString(R.string.overlay_camera_sensitivity_reset),
                                tint = Color.White,
                                modifier = Modifier.size(16.dp)
                            )
                        }
                        
                        // Close button
                        IconButton(
                            onClick = { hide() },
                            modifier = Modifier.size(32.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.Close,
                                contentDescription = parentService.getString(R.string.overlay_camera_sensitivity_close),
                                tint = Color.White,
                                modifier = Modifier.size(16.dp)
                            )
                        }
                    }
                }
                
                Divider(color = Color.Gray, thickness = 1.dp)
                
                // Sensitivity controls
                SensitivityControl(
                    label = parentService.getString(R.string.config_camera_sensitivity_movement),
                    value = movementSensitivity,
                    onValueChange = { 
                        movementSensitivity = it
                        sendSensitivityUpdate()
                    }
                )
                
                SensitivityControl(
                    label = parentService.getString(R.string.config_camera_sensitivity_vertical),
                    value = verticalSensitivity,
                    onValueChange = { 
                        verticalSensitivity = it
                        sendSensitivityUpdate()
                    }
                )
                
                SensitivityControl(
                    label = parentService.getString(R.string.config_camera_sensitivity_fov),
                    value = fovSensitivity,
                    onValueChange = { 
                        fovSensitivity = it
                        sendSensitivityUpdate()
                    }
                )
                
                SensitivityControl(
                    label = parentService.getString(R.string.config_camera_sensitivity_rotation),
                    value = rotationSensitivity,
                    onValueChange = { 
                        rotationSensitivity = it
                        sendSensitivityUpdate()
                    }
                )
            }
        }
    }

    @Composable
    private fun SensitivityControl(
        label: String,
        value: Float,
        onValueChange: (Float) -> Unit,
        step: Float = 0.01f,
        minValue: Float = 0.1f,
        maxValue: Float = 5.0f
    ) {
        var isLongPressing by remember { mutableStateOf(false) }
        val coroutineScope = rememberCoroutineScope()
        
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Text(
                text = label,
                fontSize = 12.sp,
                color = Color.White
            )
            
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(6.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                // Decrease button
                ElevatedButton(
                    onClick = {
                        val newValue = (value - step).coerceIn(minValue, maxValue)
                        onValueChange(newValue)
                    },
                    modifier = Modifier
                        .size(36.dp)
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
                        containerColor = Color.Gray.copy(alpha = 0.8f),
                        contentColor = Color.White
                    ),
                    shape = RoundedCornerShape(4.dp),
                    elevation = ButtonDefaults.elevatedButtonElevation(defaultElevation = 2.dp),
                    contentPadding = PaddingValues(0.dp)
                ) {
                    Text(
                        text = "−",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        textAlign = TextAlign.Center
                    )
                }
                
                // Value input field
                OutlinedTextField(
                    value = String.format("%.2f", value),
                    onValueChange = { newText ->
                        try {
                            val newValue = newText.toFloat().coerceIn(minValue, maxValue)
                            onValueChange(newValue)
                        } catch (e: NumberFormatException) {
                            // Ignore invalid input
                        }
                    },
                    modifier = Modifier
                        .weight(1f)
                        .height(36.dp),
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal),
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = Color.White,
                        unfocusedTextColor = Color.White,
                        focusedBorderColor = Color.White.copy(alpha = 0.8f),
                        unfocusedBorderColor = Color.Gray.copy(alpha = 0.6f)
                    ),
                    singleLine = true,
                    textStyle = LocalTextStyle.current.copy(
                        fontSize = 12.sp,
                        textAlign = TextAlign.Center
                    )
                )
                
                // Increase button
                ElevatedButton(
                    onClick = {
                        val newValue = (value + step).coerceIn(minValue, maxValue)
                        onValueChange(newValue)
                    },
                    modifier = Modifier
                        .size(36.dp)
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
                        containerColor = Color.Gray.copy(alpha = 0.8f),
                        contentColor = Color.White
                    ),
                    shape = RoundedCornerShape(4.dp),
                    elevation = ButtonDefaults.elevatedButtonElevation(defaultElevation = 2.dp),
                    contentPadding = PaddingValues(0.dp)
                ) {
                    Text(
                        text = "+",
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        textAlign = TextAlign.Center
                    )
                }
            }
        }
    }
    
    private fun resetAllToDefault() {
        movementSensitivity = 1.0f
        verticalSensitivity = 1.0f
        fovSensitivity = 1.0f
        rotationSensitivity = 1.0f
        sendSensitivityUpdate()
    }
    
    private fun sendSensitivityUpdate() {
        try {
            // Save to SharedPreferences
            val sharedPrefs = parentService.getSharedPreferences("camera_sensitivity", android.content.Context.MODE_PRIVATE)
            with(sharedPrefs.edit()) {
                putFloat("movement", movementSensitivity)
                putFloat("vertical", verticalSensitivity)
                putFloat("fov", fovSensitivity)
                putFloat("rotation", rotationSensitivity)
                apply()
            }
            
            // Send to native layer
            val configUpdate = ConfigUpdate.newBuilder()
                .setUpdateType(ConfigUpdateType.PARTIAL_UPDATE)
                .setCameraMovementSensitivity(movementSensitivity)
                .setCameraVerticalSensitivity(verticalSensitivity)
                .setCameraFovSensitivity(fovSensitivity)
                .setCameraRotationSensitivity(rotationSensitivity)
                .build()
            
            if (parentService.getSocketServerInstance().isConnected()) {
                parentService.getSocketServerInstance().sendMessage(MessageType.CONFIG_UPDATE, configUpdate)
                Log.d(TAG, "Sensitivity update sent: movement=$movementSensitivity, vertical=$verticalSensitivity, fov=$fovSensitivity, rotation=$rotationSensitivity")
            } else {
                Log.w(TAG, "Socket server not connected, cannot send sensitivity update")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error sending sensitivity update", e)
        }
    }

    fun destroy() {
        hide()
    }
}