package io.github.chocolzs.linkura.localify.ui.components

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.chocolzs.linkura.localify.ui.components.base.AutoSizeText
import kotlin.math.abs

@Composable
fun GakuSlider(
    modifier: Modifier = Modifier,
    text: String = "",
    value: Float = 1.0f,
    valueRange: ClosedFloatingPointRange<Float> = 0.5f..10.0f,
    steps: Int = 0,
    onValueChange: (Float) -> Unit = {},
    markerValues: List<Float> = listOf(0.5f, 0.75f, 1.0f, 2.0f, 5.0f, 10.0f)
) {
    val density = LocalDensity.current
    var sliderValue by remember(value) { mutableFloatStateOf(value) }
    
    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        // Header with title and current value
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            if (text.isNotEmpty()) {
                AutoSizeText(
                    text = text, 
                    fontSize = 16.sp,
                    modifier = Modifier.weight(1f)
                )
            }
            Text(
                text = String.format("%.2fx", sliderValue),
                fontSize = 14.sp,
                color = MaterialTheme.colorScheme.primary,
                textAlign = TextAlign.End
            )
        }
        
        // Slider with markers
        Box(
            modifier = Modifier.fillMaxWidth()
        ) {
            // Custom overlay for markers
            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(40.dp)
            ) {
                drawMarkers(
                    markerValues = markerValues,
                    valueRange = valueRange,
                    currentValue = sliderValue,
                    density = density
                )
            }
            
            // Main slider
            Slider(
                value = sliderValue,
                onValueChange = { newValue ->
                    // Snap to markers if close enough
                    val snappedValue = snapToMarker(newValue, markerValues, snapThreshold = 0.05f)
                    sliderValue = snappedValue
                    onValueChange(snappedValue)
                },
                valueRange = valueRange,
                steps = steps,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 8.dp),
                colors = SliderDefaults.colors(
                    thumbColor = MaterialTheme.colorScheme.primary,
                    activeTrackColor = MaterialTheme.colorScheme.primary,
                    inactiveTrackColor = Color(0xFFCFD8DC)
                )
            )
        }
        
        // Quick preset buttons aligned with markers
        Box(
            modifier = Modifier.fillMaxWidth()
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                markerValues.forEach { markerValue ->
                    Box(
                        modifier = Modifier
                            .size(width = 40.dp, height = 32.dp)
                            .clickable(
                                interactionSource = remember { MutableInteractionSource() },
                                indication = null
                            ) {
                                sliderValue = markerValue
                                onValueChange(markerValue)
                            },
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = when (markerValue) {
                                0.5f -> "0.5"
                                0.75f -> "0.75"
                                1.0f -> "1.0"
                                2.0f -> "2.0"
                                5.0f -> "5.0"
                                10.0f -> "10.0"
                                else -> String.format("%.1f", markerValue).replace(".0", "")
                            },
                            fontSize = 10.sp,
                            color = if (abs(sliderValue - markerValue) < 0.01f) {
                                MaterialTheme.colorScheme.primary
                            } else {
                                MaterialTheme.colorScheme.onSurface.copy(alpha = 0.6f)
                            },
                            textAlign = TextAlign.Center
                        )
                    }
                }
            }
        }
    }
}

private fun DrawScope.drawMarkers(
    markerValues: List<Float>,
    valueRange: ClosedFloatingPointRange<Float>,
    currentValue: Float,
    density: androidx.compose.ui.unit.Density
) {
    val trackHeight = with(density) { 4.dp.toPx() }
    val trackY = size.height / 2
    val trackWidth = size.width - with(density) { 32.dp.toPx() } // Account for thumb padding
    val trackStart = with(density) { 16.dp.toPx() }
    
    markerValues.forEach { markerValue ->
        val progress = (markerValue - valueRange.start) / (valueRange.endInclusive - valueRange.start)
        val markerX = trackStart + progress * trackWidth
        
        // Draw marker line
        val markerColor = if (markerValue == 1.0f) {
            Color(0xFF4CAF50) // Green for 1.0x
        } else if (abs(currentValue - markerValue) < 0.01f) {
            Color(0xFF2196F3) // Blue for current value
        } else {
            Color(0xFFBDBDBD) // Gray for other markers
        }
        
        val markerHeight = if (markerValue == 1.0f) {
            with(density) { 12.dp.toPx() }
        } else {
            with(density) { 8.dp.toPx() }
        }
        
        drawLine(
            color = markerColor,
            start = androidx.compose.ui.geometry.Offset(markerX, trackY - markerHeight / 2),
            end = androidx.compose.ui.geometry.Offset(markerX, trackY + markerHeight / 2),
            strokeWidth = with(density) { 2.dp.toPx() }
        )
    }
}

private fun snapToMarker(
    value: Float,
    markerValues: List<Float>,
    snapThreshold: Float
): Float {
    return markerValues.firstOrNull { marker ->
        abs(value - marker) <= snapThreshold
    } ?: value
}

@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO)
@Composable
fun GakuSliderPreview() {
    GakuSlider(
        text = "Playback Speed",
        value = 1.0f,
        onValueChange = {}
    )
}