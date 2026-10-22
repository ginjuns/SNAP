package com.example.snapapp.ui.user

import android.content.Context
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.example.snapapp.R
import com.example.snapapp.ServerClient
import org.json.JSONObject

/* 이벤트 목록: 배너 이미지와 포스터 이미지 */
private val events = listOf(
    R.drawable.event_banner1 to R.drawable.event_poster1,
    R.drawable.event_banner2 to R.drawable.event_poster2
)

@Composable
fun HomeScreen(onCharge: () -> Unit) {
    val context = LocalContext.current
    val backgroundColor = Color(0xFFEDF0E8)

    var name by remember {
        mutableStateOf("")
    }

    var balance by remember {
        mutableStateOf<Int?>(null)
    }

    var poster by remember {
        mutableStateOf<Int?>(null)
    }

    /* 홈 화면이 표시될 때 서버에서 사용자 이름과 잔액 조회 */
    LaunchedEffect(Unit) {
        val userNo = context.getSharedPreferences(
            "loginPrefs",
            Context.MODE_PRIVATE
        ).getInt("userNo", 0)

        try {
            val res = ServerClient.request(
                JSONObject()
                    .put("cmd", "LOGIN")
                    .put("userId", userNo)
            )

            if (res.optBoolean("ok")) {
                val data = res.getJSONObject("data")

                name = data.optString("name")
                balance = data.optInt("balance")
            }
        } catch (e: Exception) {
            // 연결 실패 시 잔액 "- 원"으로 표시
        }
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(backgroundColor)
    ) {
        /* 상단 배경 이미지 */
        Image(
            painter = painterResource(id = R.drawable.home),
            contentDescription = null,
            modifier = Modifier
                .fillMaxWidth()
                .height(340.dp)
                .align(Alignment.TopCenter),
            contentScale = ContentScale.Crop
        )

        /* 이미지 아래 배경색으로 이어지는 그라데이션 */
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(380.dp)
                .align(Alignment.TopCenter)
                .background(
                    brush = Brush.verticalGradient(
                        colorStops = arrayOf(
                            0.0f to Color.Transparent,
                            0.35f to Color.Transparent,
                            0.65f to backgroundColor.copy(alpha = 0.65f),
                            1.0f to backgroundColor
                        )
                    )
                )
        )

        /* 홈 화면 콘텐츠 */
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .statusBarsPadding()
                .padding(horizontal = 24.dp)
        ) {
            /* 앱 로고 텍스트 */
            Text(
                text = "SNAP",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                letterSpacing = 2.sp,
                modifier = Modifier.padding(top = 24.dp)
            )

            Spacer(modifier = Modifier.height(32.dp))

            /* 사용자 환영 메시지 */
            Text(
                text = "${name} 님!",
                fontSize = 28.sp,
                fontWeight = FontWeight.Medium
            )

            Text(
                text = "오늘도 반갑습니다.",
                fontSize = 28.sp
            )

            Spacer(modifier = Modifier.height(32.dp))

            /* 보유 잔액과 충전 버튼 */
            Row(
                verticalAlignment = Alignment.Bottom,
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(
                    modifier = Modifier.weight(1f)
                ) {
                    Text(
                        text = "보유 잔액",
                        fontSize = 16.sp
                    )

                    Text(
                        text = balance?.let {
                            "%,d 원".format(it)
                        } ?: "- 원",
                        fontSize = 28.sp,
                        fontWeight = FontWeight.Bold
                    )
                }

                Button(
                    onClick = onCharge
                ) {
                    Text(
                        text = "충전",
                        fontSize = 17.sp
                    )
                }
            }

            /* 이벤트 배너 목록 */
            Text(
                text = "이벤트",
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(
                    top = 40.dp,
                    bottom = 12.dp
                )
            )

            events.forEach { (banner, eventPoster) ->
                Image(
                    painter = painterResource(id = banner),
                    contentDescription = "이벤트 배너",
                    contentScale = ContentScale.Crop,
                    modifier = Modifier
                        .fillMaxWidth()
                        .aspectRatio(2f / 1f)
                        .clip(RoundedCornerShape(16.dp))
                        .clickable {
                            // 배너를 누르면 해당 포스터 표시
                            poster = eventPoster
                        }
                )

                Spacer(modifier = Modifier.height(16.dp))
            }

            Spacer(modifier = Modifier.height(8.dp))
        }
    }

    poster?.let { posterId -> // 이벤트 포스터 전체 화면 표시
        Dialog(
            onDismissRequest = {
                poster = null
            },
            properties = DialogProperties(
                usePlatformDefaultWidth = false
            )
        ) {
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .background(Color.White)
                    .systemBarsPadding()
            ) {
                // 포스터 이미지를 화면 너비에 맞춰 표시
                Column(
                    modifier = Modifier
                        .fillMaxSize()
                        .verticalScroll(rememberScrollState())
                ) {
                    Image(
                        painter = painterResource(id = posterId),
                        contentDescription = "이벤트 포스터",
                        contentScale = ContentScale.FillWidth,
                        modifier = Modifier.fillMaxWidth()
                    )
                }

                /* 포스터 닫기 버튼 */
                IconButton(
                    onClick = {
                        poster = null
                    },
                    modifier = Modifier
                        .align(Alignment.TopEnd)
                        .padding(8.dp)
                        .background(
                            Color.Black.copy(alpha = 0.4f),
                            RoundedCornerShape(50)
                        )
                ) {
                    Icon(
                        imageVector = Icons.Filled.Close,
                        contentDescription = "닫기",
                        tint = Color.White
                    )
                }
            }
        }
    }
}