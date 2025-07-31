package io.github.chocolzs.linkura.localify.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.text.TextStyle
import kotlinx.coroutines.launch
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.ui.theme.HasuBg
import io.github.chocolzs.linkura.localify.ui.theme.HasuStrong
import androidx.compose.foundation.Canvas
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.center
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.foundation.gestures.detectDragGestures
import kotlin.math.*
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlin.math.roundToInt

import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.text.style.TextOverflow
import kotlin.math.abs

@Composable
fun ColorPicker(
    onColorSelected: (Color) -> Unit,
    onDismiss: () -> Unit,
    initialColor: Color = Color.Black
) {
    val configuration = LocalConfiguration.current
    val isLandscape = configuration.orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE
    
    var selectedColor by remember { mutableStateOf(initialColor) }
    var redValue by remember { mutableStateOf((initialColor.red * 255).roundToInt()) }
    var greenValue by remember { mutableStateOf((initialColor.green * 255).roundToInt()) }
    var blueValue by remember { mutableStateOf((initialColor.blue * 255).roundToInt()) }
    var alphaValue by remember { mutableStateOf((initialColor.alpha * 255).roundToInt()) }
    var hexValue by remember { mutableStateOf(colorToHex(initialColor)) }
    
    // HSL values - don't update on every selectedColor change to prevent flickering
    var hueValue by remember { mutableStateOf(0f) }
    var saturationValue by remember { mutableStateOf(0f) }
    var lightnessValue by remember { mutableStateOf(0f) }
    
    // Initialize HSL values once
    LaunchedEffect(Unit) {
        val hsl = colorToHSL(initialColor)
        hueValue = hsl.first
        saturationValue = hsl.second
        lightnessValue = hsl.third
    }
    
    var colorMode by remember { mutableStateOf(ColorMode.RGB) }

    // Preset colors - exactly 6 colors as requested
    val presetColors = listOf(
        Color(0xFF00FF00),  // Pure Green
        Color(0xFF0000FF),  // Pure Blue
        Color(0xFFFF0000),  // Pure Red
        Color(0xFF000000),  // Black
        Color(0xFFFFFFFF),  // White
        Color(0xFF3AC3FA)   // Custom Blue #3ac3fa
    )

    Card(
            modifier = Modifier
                .fillMaxWidth(if (isLandscape) 0.6f else 0.9f)
                .wrapContentHeight(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(
                containerColor = MaterialTheme.colorScheme.surface
            )
        ) {
            if (isLandscape) {
                // Horizontal layout for landscape
                Row(
                    modifier = Modifier
                        .fillMaxSize()
                        .padding(16.dp),
                    horizontalArrangement = Arrangement.spacedBy(16.dp)
                ) {
                    // Left side: Color selection and buttons
                    Column(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight(),
                        verticalArrangement = Arrangement.SpaceBetween
                    ) {
                        Column(
                            modifier = Modifier.weight(1f)
                        ) {
                            // Visual color picker for landscape
                            Row(
                                modifier = Modifier.fillMaxWidth().height(80.dp).padding(bottom = 12.dp),
                                horizontalArrangement = Arrangement.spacedBy(16.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                SaturationLightnessPicker(
                                    modifier = Modifier.weight(1f).fillMaxHeight(),
                                    hue = hueValue,
                                    selectedColor = selectedColor,
                                    onSaturationValueChange = { saturation, value ->
                                        val color = hsvToColor(hueValue, saturation, value, alphaValue / 255f)
                                        selectedColor = color
                                        redValue = (color.red * 255).roundToInt()
                                        greenValue = (color.green * 255).roundToInt()
                                        blueValue = (color.blue * 255).roundToInt()
                                        hexValue = colorToHex(color)
                                        val (_, newSaturation, newLightness) = colorToHSL(color)
                                        saturationValue = newSaturation
                                        lightnessValue = newLightness
                                    }
                                )
                                HueRingPicker(
                                    hue = hueValue,
                                    selectedColor = selectedColor,
                                    onHueChange = { hue ->
                                        hueValue = hue
                                        val color = hslToColor(hue, saturationValue, lightnessValue, alphaValue / 255f)
                                        selectedColor = color
                                        redValue = (color.red * 255).roundToInt()
                                        greenValue = (color.green * 255).roundToInt()
                                        blueValue = (color.blue * 255).roundToInt()
                                        hexValue = colorToHex(color)
                                    }
                                )
                            }
                            
                            // Color selection area with sliders
                            ColorSelectionArea(
                                modifier = Modifier.weight(1f),
                            selectedColor = selectedColor,
                            redValue = redValue,
                            greenValue = greenValue,
                            blueValue = blueValue,
                            alphaValue = alphaValue,
                            hexValue = hexValue,
                            colorMode = colorMode,
                            hueValue = hueValue,
                            saturationValue = saturationValue,
                            lightnessValue = lightnessValue,
                            onRedChange = { 
                                redValue = it
                                updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                    selectedColor = color
                                    hexValue = hex
                                    val hsl = colorToHSL(color)
                                    hueValue = hsl.first
                                    saturationValue = hsl.second
                                    lightnessValue = hsl.third
                                }
                            },
                            onGreenChange = { 
                                greenValue = it
                                updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                    selectedColor = color
                                    hexValue = hex
                                    val hsl = colorToHSL(color)
                                    hueValue = hsl.first
                                    saturationValue = hsl.second
                                    lightnessValue = hsl.third
                                }
                            },
                            onBlueChange = { 
                                blueValue = it
                                updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                    selectedColor = color
                                    hexValue = hex
                                    val hsl = colorToHSL(color)
                                    hueValue = hsl.first
                                    saturationValue = hsl.second
                                    lightnessValue = hsl.third
                                }
                            },
                            onAlphaChange = { 
                                alphaValue = it
                                updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                    selectedColor = color
                                    hexValue = hex
                                }
                            },
                            onHexChange = { hex ->
                                hexValue = hex
                                hexToColor(hex)?.let { color ->
                                    selectedColor = color
                                    redValue = (color.red * 255).roundToInt()
                                    greenValue = (color.green * 255).roundToInt()
                                    blueValue = (color.blue * 255).roundToInt()
                                    alphaValue = (color.alpha * 255).roundToInt()
                                    val hsl = colorToHSL(color)
                                    hueValue = hsl.first
                                    saturationValue = hsl.second
                                    lightnessValue = hsl.third
                                }
                            },
                            onHueChange = { h ->
                                hueValue = h
                                val color = hslToColor(hueValue, saturationValue, lightnessValue, alphaValue / 255f)
                                selectedColor = color
                                redValue = (color.red * 255).roundToInt()
                                greenValue = (color.green * 255).roundToInt()
                                blueValue = (color.blue * 255).roundToInt()
                                hexValue = colorToHex(color)
                            },
                            onSaturationChange = { s ->
                                saturationValue = s
                                val color = hslToColor(hueValue, saturationValue, lightnessValue, alphaValue / 255f)
                                selectedColor = color
                                redValue = (color.red * 255).roundToInt()
                                greenValue = (color.green * 255).roundToInt()
                                blueValue = (color.blue * 255).roundToInt()
                                hexValue = colorToHex(color)
                            },
                            onLightnessChange = { l ->
                                lightnessValue = l
                                val color = hslToColor(hueValue, saturationValue, lightnessValue, alphaValue / 255f)
                                selectedColor = color
                                redValue = (color.red * 255).roundToInt()
                                greenValue = (color.green * 255).roundToInt()
                                blueValue = (color.blue * 255).roundToInt()
                                hexValue = colorToHex(color)
                            },
                            onColorModeChange = { mode ->
                                colorMode = mode
                            }
                            )
                        }
                        
                        // Buttons at bottom of left column
                        ButtonArea(
                            onConfirm = {
                                onColorSelected(selectedColor)
                                onDismiss()
                            },
                            onCancel = onDismiss
                        )
                    }
                    
                    // Right side: Preset colors in vertical arrangement
                    Column(
                        modifier = Modifier
                            .width(48.dp)
                            .fillMaxHeight()
                    ) {
                        Text(
                            text = stringResource(R.string.overlay_color_picker_presets),
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Bold,
                            modifier = Modifier.padding(bottom = 6.dp),
                            textAlign = TextAlign.Center
                        )
                        
                        // Use LazyColumn for dynamic height calculation
                        BoxWithConstraints(
                            modifier = Modifier.weight(1f)
                        ) {
                            val availableHeight = maxHeight
                            val itemCount = presetColors.size
                            val spacing = 4.dp
                            val totalSpacing = spacing * (itemCount - 1)
                            val itemHeight = (availableHeight - totalSpacing) / itemCount
                            
                            Column(
                                modifier = Modifier.fillMaxSize(),
                                verticalArrangement = Arrangement.spacedBy(spacing)
                            ) {
                                presetColors.forEach { color ->
                                    Box(
                                        modifier = Modifier
                                            .fillMaxWidth()
                                            .height(itemHeight)
                                            .clip(RoundedCornerShape(4.dp))
                                            .background(color)
                                            .border(0.5.dp, Color.Gray, RoundedCornerShape(4.dp))
                                            .clickable { 
                                                selectedColor = color
                                                redValue = (color.red * 255).roundToInt()
                                                greenValue = (color.green * 255).roundToInt()
                                                blueValue = (color.blue * 255).roundToInt()
                                                alphaValue = (color.alpha * 255).roundToInt()
                                                hexValue = colorToHex(color)
                                                val hsl = colorToHSL(color)
                                                hueValue = hsl.first
                                                saturationValue = hsl.second
                                                lightnessValue = hsl.third
                                            }
                                    )
                                }
                            }
                        }
                    }
                }
            } else {
                // Vertical layout for portrait
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .verticalScroll(rememberScrollState())
                        .padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // Color selection area
                    ColorSelectionArea(
                        selectedColor = selectedColor,
                        redValue = redValue,
                        greenValue = greenValue,
                        blueValue = blueValue,
                        alphaValue = alphaValue,
                        hexValue = hexValue,
                        colorMode = colorMode,
                        hueValue = hueValue,
                        saturationValue = saturationValue,
                        lightnessValue = lightnessValue,
                        onRedChange = { 
                            redValue = it
                            updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                selectedColor = color
                                hexValue = hex
                                val hsl = colorToHSL(color)
                                hueValue = hsl.first
                                saturationValue = hsl.second
                                lightnessValue = hsl.third
                            }
                        },
                        onGreenChange = { 
                            greenValue = it
                            updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                selectedColor = color
                                hexValue = hex
                                val hsl = colorToHSL(color)
                                hueValue = hsl.first
                                saturationValue = hsl.second
                                lightnessValue = hsl.third
                            }
                        },
                        onBlueChange = { 
                            blueValue = it
                            updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                selectedColor = color
                                hexValue = hex
                                val hsl = colorToHSL(color)
                                hueValue = hsl.first
                                saturationValue = hsl.second
                                lightnessValue = hsl.third
                            }
                        },
                        onAlphaChange = { 
                            alphaValue = it
                            updateColorFromRGBA(redValue, greenValue, blueValue, alphaValue) { color, hex ->
                                selectedColor = color
                                hexValue = hex
                            }
                        },
                        onHexChange = { hex ->
                            hexValue = hex
                            hexToColor(hex)?.let { color ->
                                selectedColor = color
                                redValue = (color.red * 255).roundToInt()
                                greenValue = (color.green * 255).roundToInt()
                                blueValue = (color.blue * 255).roundToInt()
                                alphaValue = (color.alpha * 255).roundToInt()
                                val hsl = colorToHSL(color)
                                hueValue = hsl.first
                                saturationValue = hsl.second
                                lightnessValue = hsl.third
                            }
                        },
                        onHueChange = { h ->
                            hueValue = h
                            val color = hslToColor(hueValue, saturationValue, lightnessValue, alphaValue / 255f)
                            selectedColor = color
                            redValue = (color.red * 255).roundToInt()
                            greenValue = (color.green * 255).roundToInt()
                            blueValue = (color.blue * 255).roundToInt()
                            hexValue = colorToHex(color)
                        },
                        onSaturationChange = { s ->
                            saturationValue = s
                            val color = hslToColor(hueValue, saturationValue, lightnessValue, alphaValue / 255f)
                            selectedColor = color
                            redValue = (color.red * 255).roundToInt()
                            greenValue = (color.green * 255).roundToInt()
                            blueValue = (color.blue * 255).roundToInt()
                            hexValue = colorToHex(color)
                        },
                        onLightnessChange = { l ->
                            lightnessValue = l
                            val color = hslToColor(hueValue, saturationValue, lightnessValue, alphaValue / 255f)
                            selectedColor = color
                            redValue = (color.red * 255).roundToInt()
                            greenValue = (color.green * 255).roundToInt()
                            blueValue = (color.blue * 255).roundToInt()
                            hexValue = colorToHex(color)
                        },
                        onColorModeChange = { mode ->
                            colorMode = mode
                        }
                    )
                    
                    // Preset colors
                    PresetColorsArea(
                        presetColors = presetColors,
                        onColorSelect = { color ->
                            selectedColor = color
                            redValue = (color.red * 255).roundToInt()
                            greenValue = (color.green * 255).roundToInt()
                            blueValue = (color.blue * 255).roundToInt()
                            alphaValue = (color.alpha * 255).roundToInt()
                            hexValue = colorToHex(color)
                            val hsl = colorToHSL(color)
                            hueValue = hsl.first
                            saturationValue = hsl.second
                            lightnessValue = hsl.third
                        }
                    )
                    
                    // Buttons
                    ButtonArea(
                        onConfirm = {
                            onColorSelected(selectedColor)
                            onDismiss()
                        },
                        onCancel = onDismiss
                    )
                }
            }
        }
}

// Enum for color modes
enum class ColorMode { RGB, HSL }

@Composable
private fun ColorSelectionArea(
    selectedColor: Color,
    redValue: Int,
    greenValue: Int,
    blueValue: Int,
    alphaValue: Int,
    hexValue: String,
    onRedChange: (Int) -> Unit,
    onGreenChange: (Int) -> Unit,
    onBlueChange: (Int) -> Unit,
    onAlphaChange: (Int) -> Unit,
    onHexChange: (String) -> Unit,
    colorMode: ColorMode = ColorMode.RGB,
    hueValue: Float = 0f,
    saturationValue: Float = 0f,
    lightnessValue: Float = 0f,
    onHueChange: ((Float) -> Unit)? = null,
    onSaturationChange: ((Float) -> Unit)? = null,
    onLightnessChange: ((Float) -> Unit)? = null,
    onColorModeChange: ((ColorMode) -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    Column(modifier = modifier) {
        // Title row with color preview and hex
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = stringResource(R.string.overlay_color_picker_title),
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Bold
                )
                
                // Small color preview
                Box(
                    modifier = Modifier
                        .size(24.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(selectedColor)
                        .border(1.dp, Color.Gray, RoundedCornerShape(4.dp))
                )
            }
            
            // Hex display as text
            Text(
                text = "#${hexValue}",
                fontSize = 12.sp,
                fontWeight = FontWeight.Medium,
                modifier = Modifier
                    .background(
                        MaterialTheme.colorScheme.surfaceVariant,
                        RoundedCornerShape(4.dp)
                    )
                    .padding(horizontal = 8.dp, vertical = 4.dp)
            )
        }
        
        // Color mode tabs
        if (onColorModeChange != null) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 8.dp),
                horizontalArrangement = Arrangement.spacedBy(4.dp)
            ) {
                ColorMode.values().forEach { mode ->
                    FilterChip(
                        selected = colorMode == mode,
                        onClick = { onColorModeChange(mode) },
                        label = { Text(mode.name, fontSize = 11.sp) },
                        modifier = Modifier.height(32.dp)
                    )
                }
            }
        }
        
        Spacer(modifier = Modifier.height(8.dp))
        
        // Color sliders based on mode
        when (colorMode) {
            ColorMode.RGB -> {
                ColorSlider("R", redValue, Color.Red, 0f..255f) { onRedChange(it.toInt()) }
                ColorSlider("G", greenValue, Color.Green, 0f..255f) { onGreenChange(it.toInt()) }
                ColorSlider("B", blueValue, Color.Blue, 0f..255f) { onBlueChange(it.toInt()) }
            }
            ColorMode.HSL -> {
                if (onHueChange != null && onSaturationChange != null && onLightnessChange != null) {
                    ColorSlider("H", hueValue, Color.Red, 0f..360f) { onHueChange(it) }
                    ColorSlider("S", saturationValue * 100, Color.Gray, 0f..100f) { onSaturationChange(it / 100f) }
                    ColorSlider("L", lightnessValue * 100, Color.Gray, 0f..100f) { onLightnessChange(it / 100f) }
                }
            }
        }
        
        ColorSlider("A", alphaValue, Color.Gray, 0f..255f) { onAlphaChange(it.toInt()) }
    }
}

@Composable
private fun ColorSlider(
    label: String,
    value: Number,
    color: Color,
    valueRange: ClosedFloatingPointRange<Float> = 0f..255f,
    onValueChange: (Float) -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp)
    ) {
        Text(
            text = label,
            fontSize = 12.sp,
            modifier = Modifier.width(16.dp),
            fontWeight = FontWeight.Medium
        )
        
        Slider(
            value = value.toFloat(),
            onValueChange = onValueChange,
            valueRange = valueRange,
            modifier = Modifier
                .weight(1f)
                .height(20.dp),
            colors = SliderDefaults.colors(
                thumbColor = color,
                activeTrackColor = color
            )
        )
        
        Text(
            text = value.toInt().toString(),
            fontSize = 12.sp,
            modifier = Modifier.width(32.dp),
            textAlign = TextAlign.End
        )
    }
}

@Composable
private fun PresetColorsArea(
    presetColors: List<Color>,
    onColorSelect: (Color) -> Unit
) {
    Column {
        Text(
            text = stringResource(R.string.overlay_color_picker_preset_colors),
            fontSize = 14.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(bottom = 6.dp)
        )
        
        // Single row of preset colors
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            presetColors.forEach { color ->
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .aspectRatio(1f)
                        .clip(RoundedCornerShape(4.dp))
                        .background(color)
                        .border(0.5.dp, Color.Gray, RoundedCornerShape(4.dp))
                        .clickable { onColorSelect(color) }
                )
            }
        }
    }
}

@Composable
private fun ButtonArea(
    onConfirm: () -> Unit,
    onCancel: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        GakuButton(
            onClick = onCancel,
            text = stringResource(R.string.overlay_color_picker_cancel),
            modifier = Modifier
                .weight(1f)
                .height(48.dp),
            bgColors = listOf(Color(0xFFF9F9F9), Color(0xFFF0F0F0)),
            textColor = Color(0xFF111111)
        )
   
        GakuButton(
            onClick = onConfirm,
            text = stringResource(R.string.overlay_color_picker_confirm),
            modifier = Modifier
                .weight(1f)
                .height(48.dp),
        )
    }
}

@Composable
private fun SaturationLightnessPicker(
    hue: Float,
    selectedColor: Color,
    onSaturationValueChange: (Float, Float) -> Unit,
    modifier: Modifier = Modifier
) {
    BoxWithConstraints(modifier = modifier) {
        val width = constraints.maxWidth.toFloat()
        val height = constraints.maxHeight.toFloat()

        val horizontalGradient = Brush.horizontalGradient(
            colors = listOf(Color.White, hsvToColor(hue, 1f, 1f))
        )
        val verticalGradient = Brush.verticalGradient(
            colors = listOf(Color.Transparent, Color.Black)
        )

        var (h, s, v) = colorToHSV(selectedColor)

        Canvas(modifier = Modifier.fillMaxSize().pointerInput(Unit) {
            detectDragGestures {
                change, _ ->
                val newSaturation = (change.position.x / width).coerceIn(0f, 1f)
                val newValue = 1f - (change.position.y / height).coerceIn(0f, 1f)
                onSaturationValueChange(newSaturation, newValue)
            }
        }) {
            drawRect(brush = horizontalGradient)
            drawRect(brush = verticalGradient)

            // Draw indicator
            val indicatorX = s * width
            val indicatorY = (1f - v) * height
            drawCircle(
                color = Color.White,
                radius = 8f,
                center = Offset(indicatorX, indicatorY),
                style = Stroke(width = 2f)
            )
            drawCircle(
                color = selectedColor,
                radius = 6f,
                center = Offset(indicatorX, indicatorY)
            )
        }
    }
}

@Composable
private fun HueRingPicker(
    hue: Float,
    selectedColor: Color,
    onHueChange: (Float) -> Unit,
    modifier: Modifier = Modifier
) {
    val ringWidth = 20.dp
    BoxWithConstraints(modifier = modifier.size(80.dp)) {
        val diameter = min(constraints.maxWidth, constraints.maxHeight).toFloat()
        val outerRadius = diameter / 2f
        val center = Offset(outerRadius, outerRadius)

        val hueColors = List(361) { i -> hslToColor(i.toFloat(), 1f, 0.5f) }
        val sweepGradient = Brush.sweepGradient(colors = hueColors, center = center)

        Canvas(modifier = Modifier.fillMaxSize().pointerInput(Unit) {
            detectDragGestures {
                change, _ ->
                val dx = change.position.x - center.x
                val dy = change.position.y - center.y
                val newHue = (atan2(dy, dx) * (180f / PI.toFloat()) + 360f) % 360f
                onHueChange(newHue)
            }
        }) {
            val ringWidthPx = ringWidth.toPx()
            val ringCenterRadius = outerRadius - ringWidthPx / 2f

            // Draw the ring
            drawCircle(
                brush = sweepGradient,
                radius = ringCenterRadius, // Draw at the center of the ring
                style = Stroke(width = ringWidthPx)
            )

            // Draw indicator
            val angle = hue * (PI.toFloat() / 180f)
            val indicatorX = center.x + cos(angle.toDouble()).toFloat() * ringCenterRadius // Position on the center of the ring
            val indicatorY = center.y + sin(angle.toDouble()).toFloat() * ringCenterRadius

            // The indicator itself
            drawCircle(
                color = Color.White,
                radius = ringWidthPx / 2f,
                center = Offset(indicatorX, indicatorY),
                style = Stroke(width = 4f)
            )
            drawCircle(
                color = selectedColor,
                radius = ringWidthPx / 2f - 2f,
                center = Offset(indicatorX, indicatorY)
            )
        }
    }
}

private fun colorToHex(color: Color): String {
    val argb = color.toArgb()
    return String.format("%08X", argb)
}

private fun hexToColor(hex: String): Color? {
    return try {
        val cleanHex = hex.replace("#", "").trim().uppercase()
        
        // Validate hex characters
        if (!cleanHex.all { it in '0'..'9' || it in 'A'..'F' }) {
            return null
        }
        
        when (cleanHex.length) {
            3 -> {
                // Convert 3-char hex to 6-char (e.g., "F0A" -> "FF00AA")
                val r = cleanHex[0].toString().repeat(2)
                val g = cleanHex[1].toString().repeat(2)
                val b = cleanHex[2].toString().repeat(2)
                Color(android.graphics.Color.parseColor("#$r$g$b"))
            }
            6 -> {
                // Standard 6-char hex color
                Color(android.graphics.Color.parseColor("#$cleanHex"))
            }
            8 -> {
                // 8-char hex with alpha
                val a = cleanHex.substring(0, 2).toInt(16)
                val r = cleanHex.substring(2, 4).toInt(16)
                val g = cleanHex.substring(4, 6).toInt(16)
                val b = cleanHex.substring(6, 8).toInt(16)
                Color(r, g, b, a)
            }
            else -> null
        }
    } catch (e: Exception) {
        null
    }
}

private fun updateColorFromRGBA(r: Int, g: Int, b: Int, a: Int, onUpdate: (Color, String) -> Unit) {
    val color = Color(r, g, b, a)
    val hex = colorToHex(color)
    onUpdate(color, hex)
}

// Color conversion functions
private fun colorToHSL(color: Color): Triple<Float, Float, Float> {
    val r = color.red
    val g = color.green
    val b = color.blue
    
    val max = maxOf(r, g, b)
    val min = minOf(r, g, b)
    val delta = max - min
    
    val lightness = (max + min) / 2f
    
    return if (delta == 0f) {
        Triple(0f, 0f, lightness)
    } else {
        val saturation = if (lightness > 0.5f) {
            delta / (2f - max - min)
        } else {
            delta / (max + min)
        }
        
        val hue = when (max) {
            r -> ((g - b) / delta + if (g < b) 6f else 0f)
            g -> ((b - r) / delta + 2f)
            else -> ((r - g) / delta + 4f)
        } * 60f
        
        Triple(hue, saturation, lightness)
    }
}

private fun hslToColor(h: Float, s: Float, l: Float, a: Float = 1f): Color {
    if (s == 0f) {
        val gray = (l * 255).toInt()
        return Color(gray, gray, gray, (a * 255).toInt())
    }
    
    val q = if (l < 0.5f) l * (1 + s) else l + s - l * s
    val p = 2 * l - q
    
    fun hueToRgb(p: Float, q: Float, t: Float): Float {
        var tNorm = t
        if (tNorm < 0) tNorm += 1f
        if (tNorm > 1) tNorm -= 1f
        return when {
            tNorm < 1f/6f -> p + (q - p) * 6f * tNorm
            tNorm < 1f/2f -> q
            tNorm < 2f/3f -> p + (q - p) * (2f/3f - tNorm) * 6f
            else -> p
        }
    }
    
    val hNorm = h / 360f
    val r = hueToRgb(p, q, hNorm + 1f/3f)
    val g = hueToRgb(p, q, hNorm)
    val b = hueToRgb(p, q, hNorm - 1f/3f)
    
    return Color(
        (r * 255).toInt().coerceIn(0, 255),
        (g * 255).toInt().coerceIn(0, 255),
        (b * 255).toInt().coerceIn(0, 255),
        (a * 255).toInt().coerceIn(0, 255)
    )
}

private fun colorToHSV(color: Color): Triple<Float, Float, Float> {
    val r = color.red
    val g = color.green
    val b = color.blue

    val max = maxOf(r, g, b)
    val min = minOf(r, g, b)
    val delta = max - min

    val hue = when {
        delta == 0f -> 0f
        max == r -> 60 * (((g - b) / delta) % 6)
        max == g -> 60 * (((b - r) / delta) + 2)
        else -> 60 * (((r - g) / delta) + 4)
    }

    val saturation = if (max == 0f) 0f else delta / max
    val value = max

    return Triple(hue, saturation, value)
}

private fun hsvToColor(h: Float, s: Float, v: Float, a: Float = 1f): Color {
    val c = v * s
    val x = c * (1 - abs((h / 60) % 2 - 1))
    val m = v - c

    val (r, g, b) = when {
        h < 60 -> Triple(c, x, 0f)
        h < 120 -> Triple(x, c, 0f)
        h < 180 -> Triple(0f, c, x)
        h < 240 -> Triple(0f, x, c)
        h < 300 -> Triple(x, 0f, c)
        else -> Triple(c, 0f, x)
    }

    return Color(
        ((r + m) * 255).toInt().coerceIn(0, 255),
        ((g + m) * 255).toInt().coerceIn(0, 255),
        ((b + m) * 255).toInt().coerceIn(0, 255),
        (a * 255).toInt().coerceIn(0, 255)
    )
}