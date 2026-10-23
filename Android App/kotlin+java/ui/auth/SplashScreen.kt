package com.example.snapapp.ui.auth

import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import com.example.snapapp.R
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

@Composable
fun SplashScreen(onFinish: () -> Unit) {
    val fade = remember { // 투명도
        Animatable(0f)
    }
    val grow = remember { // 크기
        Animatable(0.6f)
    }

    LaunchedEffect(Unit) {
        launch { // 나타나면서 커지는 시간
            fade.animateTo(
                1f, tween(800)
            )
        }
        grow.animateTo(
            1f, tween(800)
        )

        delay(1700)
        onFinish()
    }

    Box(
        contentAlignment = Alignment.Center,
        modifier = Modifier
            .fillMaxSize()
            .background(Color(0xFFFAFAFA))
    ) {
        Image(
            painter = painterResource(R.drawable.logo),
            contentDescription = "SNAP",
            modifier = Modifier
                .width(240.dp)
                .graphicsLayer {
                    alpha = fade.value
                    scaleX = grow.value
                    scaleY = grow.value
                }
        )
    }
}