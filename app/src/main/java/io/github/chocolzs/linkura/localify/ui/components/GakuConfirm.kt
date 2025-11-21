package io.github.chocolzs.linkura.localify.ui.components

import android.annotation.SuppressLint
import android.content.res.Configuration.UI_MODE_NIGHT_NO
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import io.github.chocolzs.linkura.localify.R
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.roundToInt

const val ANIMATION_TIME = 320

@Composable
fun FullScreenBoxWithAnimation(
    isVisible: Boolean,
    onDismiss: () -> Unit,
    content: @Composable () -> Unit
) {
    val animatedAlpha by animateFloatAsState(
        targetValue = if (isVisible) 0.6f else 0f, label = "animatedAlpha",
        animationSpec = tween(durationMillis = ANIMATION_TIME)
    )

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = animatedAlpha))
            .clickable {
                // isVisible2 = false
                onDismiss()
            },
        contentAlignment = Alignment.BottomCenter
    ) {
        content()
    }
}


@SuppressLint("UnusedMaterial3ScaffoldPaddingParameter")
@Composable
fun GakuGroupConfirm(
    modifier: Modifier = Modifier,
    title: String = "Title",
    maxWidth: Dp = 400.dp,
    contentPadding: Dp = 8.dp,
    rightHead: @Composable (() -> Unit)? = null,
    onHeadClick: () -> Unit = {},
    onConfirm: () -> Unit = {},
    onCancel: () -> Unit = {},
    confirmText: String? = null,
    cancelText: String? = null,
    contentHeightForAnimation: Float = 400f,
    initIsVisible: Boolean = false,
    baseModifier: Modifier = Modifier,
    content: @Composable () -> Unit
) {
    val scoop = rememberCoroutineScope()
    var isVisible by remember { mutableStateOf(initIsVisible) }
    val offsetY by animateFloatAsState(
        targetValue = if (isVisible) -35f else contentHeightForAnimation, // 控制Box移动的距离
        animationSpec = tween(durationMillis = ANIMATION_TIME), label = "offsetY"
    )

    LaunchedEffect(Unit) {
        isVisible = true
    }

    Scaffold(modifier = baseModifier.background(
        color = Color.Transparent
    ),
        containerColor = Color.Transparent) {
        FullScreenBoxWithAnimation(
            isVisible = isVisible,
            onDismiss = {
                isVisible = false
                scoop.launch {
                    delay(ANIMATION_TIME.toLong())
                    onCancel()
                }
            }
        ) {
            Box(Modifier
                .offset { IntOffset(0, offsetY.roundToInt()) }
                .widthIn(max = maxWidth)
                .clickable {  }) {
                Column(modifier = modifier.widthIn(max = maxWidth)) {
                    // Header
                    Box(
                        modifier = modifier
                            .fillMaxWidth()
                            .background(Color.Transparent)
                            .height(30.dp)
                            .clickable {
                                onHeadClick()
                            }
                    ) {
                        Image(
                            painter = painterResource(id = R.drawable.bg_sheet_title),
                            contentDescription = null,
                            // modifier = Modifier.fillMaxSize(),
                            modifier = Modifier.fillMaxSize(),
                            contentScale = ContentScale.FillBounds
                        )
                        Text(
                            text = title,
                            style = MaterialTheme.typography.titleSmall,
                            color = Color.White,
                            modifier = modifier
                                .align(Alignment.CenterStart)
                                .padding(start = (maxWidth.value * 0.043f).dp)
                        )
                        if (rightHead != null) {
                            Box(modifier = Modifier
                                .align(Alignment.CenterEnd)
                                .padding(end = (maxWidth.value * 0.1f).dp)) {
                                rightHead()
                            }
                        }
                    }

                    // Content
                    Row {
                        Spacer(modifier = Modifier.width(4.dp))
                        Box(
                            modifier = modifier
                                .shadow(
                                    4.dp, RoundedCornerShape(
                                        bottomStart = 16.dp,
                                        bottomEnd = 8.dp,
                                        topEnd = 0.dp,
                                        topStart = 0.dp
                                    )
                                )
                                .background(
                                    color = Color.White,
                                    shape = RoundedCornerShape(
                                        bottomStart = 16.dp,
                                        bottomEnd = 8.dp
                                    )
                                )
                                .padding(
                                    contentPadding + 5.dp, contentPadding, contentPadding,
                                    contentPadding
                                )
                                .fillMaxWidth()
                        ) {
                            Column {
                                content()
                                Spacer(modifier = Modifier.height(22.dp))
                            }
                        }
                    }
                    Spacer(modifier = Modifier.height(22.dp))
                }

                Box(modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()) {
                    Row(Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.Center) {
                        Spacer(modifier = Modifier.sizeIn(minWidth = 32.dp))
                        Row(
                            Modifier
                                .fillMaxWidth()
                                .weight(1f),
                            horizontalArrangement = Arrangement.Center) {
                            GakuButton(modifier = Modifier
                                .height(40.dp)
                                .sizeIn(minWidth = 100.dp),
                                text = cancelText ?: stringResource(R.string.cancel),
                                bgColors = listOf(Color(0xFFF9F9F9), Color(0xFFF0F0F0)),
                                textColor = Color(0xFF111111),
                                onClick = { scoop.launch {
                                    isVisible = false
                                    delay(ANIMATION_TIME.toLong())
                                    onCancel()
                                } })
                        }
                        Row(
                            Modifier
                                .fillMaxWidth()
                                .weight(1f),
                            horizontalArrangement = Arrangement.Center) {
                            GakuButton(modifier = Modifier
                                .height(40.dp)
                                .sizeIn(minWidth = 100.dp),
                                text = confirmText ?: stringResource(R.string.ok),
                                onClick = { scoop.launch {
                                    isVisible = false
                                    delay(ANIMATION_TIME.toLong())
                                    onConfirm()
                                } })
                        }
                        Spacer(modifier = Modifier.sizeIn(minWidth = 32.dp))
                    }
                }
            }
        }
    }
}

@Preview(showBackground = true, uiMode = UI_MODE_NIGHT_NO)
@Composable
fun PreviewGakuGroupConfirm() {
    GakuGroupConfirm(
        title = "Confirm Title",
        initIsVisible = true
    ) {
        Column {
            Text("This is the content of the GakuGroupConfirm.")
            Text("This is the content of the GakuGroupConfirm.")
        }
    }
}
