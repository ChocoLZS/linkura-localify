package io.github.chocolzs.linkura.localify.ui.components

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.ui.Alignment
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.sp
import io.github.chocolzs.linkura.localify.ui.components.base.AutoSizeText


@Composable
fun GakuSwitch(modifier: Modifier = Modifier,
               text: String = "",
               checked: Boolean = false,
               enabled: Boolean = true,
               leftPart: @Composable (() -> Unit)? = null,
               onCheckedChange: (Boolean) -> Unit = {}) {
    Row(modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically) {
        if (text.isNotEmpty()) {
            AutoSizeText(
                text = text, 
                fontSize = 16.sp,
                color = if (enabled) MaterialTheme.colorScheme.onSurface else MaterialTheme.colorScheme.onSurface.copy(alpha = 0.38f)
            )
        }
        leftPart?.invoke()
        Switch(checked = checked,
            enabled = enabled,
            onCheckedChange = { value -> onCheckedChange(value) },
            modifier = Modifier,
            colors = SwitchDefaults.colors(
                checkedThumbColor = Color(0xFFFFFFFF),
                checkedTrackColor = MaterialTheme.colorScheme.primaryContainer,

                uncheckedThumbColor = Color(0xFFFFFFFF),
                uncheckedTrackColor = Color(0xFFCFD8DC),
                uncheckedBorderColor = Color(0xFFCFD8DC),
                
                disabledCheckedThumbColor = Color(0xFFBDBDBD),
                disabledCheckedTrackColor = Color(0xFFE0E0E0),
                disabledUncheckedThumbColor = Color(0xFFBDBDBD),
                disabledUncheckedTrackColor = Color(0xFFE0E0E0),
            ))
    }
}


@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO)
@Composable
fun GakuSwitchPreview() {
    GakuSwitch(text = "Switch", checked = true)
}
