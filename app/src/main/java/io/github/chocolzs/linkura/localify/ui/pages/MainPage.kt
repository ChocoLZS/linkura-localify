package io.github.chocolzs.linkura.localify.ui.pages

import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import io.github.chocolzs.linkura.localify.MainActivity
import io.github.chocolzs.linkura.localify.R
import io.github.chocolzs.linkura.localify.getMainUIConfirmState
import io.github.chocolzs.linkura.localify.getProgramConfigState
import io.github.chocolzs.linkura.localify.models.LinkuraConfig
import io.github.chocolzs.linkura.localify.ui.components.GakuGroupConfirm
import io.github.chocolzs.linkura.localify.ui.components.ConnectionStatusIndicator
import io.github.chocolzs.linkura.localify.ui.theme.LocalifyTheme


@Composable
fun MainUI(modifier: Modifier = Modifier, context: MainActivity? = null,
           previewData: LinkuraConfig? = null) {
    val imagePainter = painterResource(R.drawable.bg_pattern)
    var versionInfo by remember {
        mutableStateOf(context?.getVersion() ?: listOf("", "Unknown"))
    }
    // val config = getConfigState(context, previewData)
    val confirmState by getMainUIConfirmState(context, null)
    val programConfig by getProgramConfigState(context)
    var showConnectionDialog by remember { mutableStateOf(false) }

    LaunchedEffect(programConfig) {
        versionInfo = context?.getVersion() ?: listOf("", "Unknown")
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFFFDFDFD))
    ) {
        val screenH = imageRepeater(
            painter = imagePainter,
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.TopCenter)
        )

        Column(
            modifier = modifier
                .fillMaxSize()
                .padding(10.dp, 10.dp, 10.dp, 0.dp),
            verticalArrangement = Arrangement.Top
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.Top
            ) {
                Column {
                    Text(text = "Linkura Localify ${versionInfo[0]}", fontSize = 18.sp)
                    Text(text = "Assets version: ${versionInfo[1]}", fontSize = 13.sp)
                }
                
                // Connection status indicator in top right
                ConnectionStatusIndicator(
                    modifier = Modifier.padding(top = 4.dp),
                    size = 20f,
                    showDialog = showConnectionDialog,
                    onDismissDialog = { showConnectionDialog = false },
                    onShowDialog = { showConnectionDialog = true }
                )
            }

            SettingsTabs(modifier, listOf(stringResource(R.string.about), stringResource(R.string.home),
                stringResource(R.string.advanced_settings), stringResource(R.string.resource_management)
                ),
                context = context, previewData = previewData, screenH = screenH)
        }

        if (confirmState.isShow) {
            GakuGroupConfirm(
                title = confirmState.title,
                onCancel = { confirmState.onCancel() },
                onConfirm = { confirmState.onConfirm() },
                confirmText = confirmState.confirmText,
                cancelText = confirmState.cancelText,
                contentHeightForAnimation = screenH.value * 1.8f
            ) {
                LazyColumn(modifier =
                Modifier.sizeIn(maxHeight = (screenH.value * 0.45f).dp)
                    .fillMaxWidth()) {
                    item {
                        Text(confirmState.content)
                    }
                }
            }
        }
    }
}


@Composable
fun imageRepeater(
    painter: Painter,
    modifier: Modifier = Modifier
): Dp {
    val density = LocalDensity.current
    val imageHeightPx = painter.intrinsicSize.height
    val imageHeightDp = with(density) { imageHeightPx.toDp() }
    var retMaxH = 1080.dp
    BoxWithConstraints(modifier = modifier) {
        retMaxH = maxHeight
        val screenHeight = maxHeight
        val repeatCount = (screenHeight / imageHeightDp).toInt() + 1

        Column {
            repeat(repeatCount) {
                Image(
                    painter = painter,
                    contentDescription = null,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(imageHeightDp)
                )
            }
        }
    }
    return retMaxH
}


@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO, widthDp = 380)
@Composable
fun MainUIPreview(modifier: Modifier = Modifier) {
    val previewConfig = LinkuraConfig()
    previewConfig.enabled = true

    LocalifyTheme {
        MainUI(previewData = previewConfig)
    }
}
