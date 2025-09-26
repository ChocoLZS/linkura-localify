package io.github.chocolzs.linkura.localify.ui.components

import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowDropDown
import androidx.compose.material3.ButtonColors
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.chocolzs.linkura.localify.ui.components.base.AutoSizeText

@Composable
fun <T> GakuSelector(
    options: List<Pair<String, T>>,
    selectedValue: T,
    onValueSelected: (T) -> Unit
) {
    var isDropdownExpanded by remember { mutableStateOf(false) }
    val selectedOption = options.find { it.second == selectedValue } ?: options.firstOrNull()

    val arrowRotation by animateFloatAsState(
        targetValue = if (isDropdownExpanded) 180f else 0f,
        animationSpec = tween(durationMillis = 300),
        label = "arrow_rotation"
    )
    Box {
        OutlinedButton(
            modifier = Modifier.height(32.dp),
            onClick = { isDropdownExpanded = true },
            colors = ButtonDefaults.outlinedButtonColors(
                contentColor = MaterialTheme.colorScheme.onPrimary
            ),
        ) {
            Text(
                fontSize = 14.sp,
                text = selectedOption?.first ?: "...",
                modifier = Modifier.padding(end = 8.dp),
            )
            Icon(
                imageVector = Icons.Filled.ArrowDropDown,
                contentDescription = null,
                modifier = Modifier.rotate(arrowRotation)
            )
        }

        DropdownMenu(
            expanded = isDropdownExpanded,
            onDismissRequest = { isDropdownExpanded = false }
        ) {
            options.forEach { (name, value) ->
                DropdownMenuItem(
                    text = { Text(name) },
                    onClick = {
                        onValueSelected(value)
                        isDropdownExpanded = false
                    }
                )
            }
        }
    }
}
