package com.example.snapapp.ui.user

import android.content.Context
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AccountCircle
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.snapapp.ServerClient
import com.example.snapapp.ui.theme.SnapBarBg
import com.example.snapapp.ui.theme.SnapGreen
import org.json.JSONObject

@Composable
fun MyPageScreen(
    onChangePassword: () -> Unit,
    onLogout: () -> Unit
) {
    val context = LocalContext.current

    /* 로그인 정보 저장소 */
    val prefs = remember {
        context.getSharedPreferences(
            "loginPrefs",
            Context.MODE_PRIVATE
        )
    }

    val userNo = remember {
        prefs.getInt("userNo", 0)
    }

    val loginId = remember {
        prefs.getString("userId", "") ?: ""
    }

    /* 사용자 정보 및 로그아웃 다이얼로그 상태 */
    var name by remember {
        mutableStateOf("")
    }

    var phone by remember {
        mutableStateOf("")
    }

    var showLogout by remember {
        mutableStateOf(false)
    }

    /* 화면 진입하면 서버에서 이름과 전화번호 조회 */
    LaunchedEffect(Unit) {
        try {
            val res = ServerClient.request(
                JSONObject()
                    .put("cmd", "LOGIN")
                    .put("userId", userNo)
            )

            if (res.optBoolean("ok")) {
                val data = res.getJSONObject("data")

                name = data.optString("name")
                phone = data.optString("phone")
            }
        } catch (e: Exception) {
            // 연결 실패 시 이름과 전화번호 "-"로 표시
        }
    }

    Column(
        modifier = Modifier.fillMaxSize()
    ) {
        /* 프로필 영역 */
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(240.dp)
        ) {
            /* 상단 배경 곡선 */
            Canvas(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(180.dp)
            ) {
                drawOval(
                    color = SnapBarBg,
                    topLeft = Offset(
                        -size.width * 0.3f,
                        -size.height
                    ),
                    size = Size(
                        size.width * 1.6f,
                        size.height * 2
                    )
                )
            }

            /* 기본 프로필 아이콘 */
            Icon(
                imageVector = Icons.Filled.AccountCircle,
                contentDescription = "프로필",
                tint = Color.LightGray,
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .size(110.dp)
                    .clip(CircleShape)
                    .background(Color.White)
            )
        }

        /* 회원 정보 */
        Column(
            modifier = Modifier.padding(
                horizontal = 24.dp,
                vertical = 8.dp
            )
        ) {
            InfoRow(
                label = "이름",
                value = name.ifBlank { "-" }
            )

            InfoRow(
                label = "아이디",
                value = loginId.ifBlank { "-" }
            )

            InfoRow(
                label = "휴대전화 번호",
                value = phone.ifBlank { "-" }
            )
        }

        /* 버튼을 화면 아래쪽에 배치 */
        Spacer(
            modifier = Modifier.weight(1f)
        )

        /* 비밀번호 변경 및 로그아웃 버튼 */
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 24.dp)
        ) {
            OutlinedButton(
                onClick = onChangePassword,
                border = BorderStroke(
                    1.dp,
                    SnapGreen
                ),
                modifier = Modifier
                    .weight(1f)
                    .height(44.dp)
            ) {
                Text(
                    text = "비밀번호 변경",
                    color = SnapGreen
                )
            }

            Button(
                onClick = {
                    showLogout = true
                },
                modifier = Modifier
                    .weight(1f)
                    .height(44.dp)
            ) {
                Text("로그아웃")
            }
        }

        /* 회원 탈퇴 안내 */
        Text(
            text = "회원 탈퇴는 고객센터로 문의해 주세요.\n고객센터 번호: 1234-5678", // 회원 탈퇴 기능은 안 만듦...
            fontSize = 13.sp,
            color = Color.Gray,
            modifier = Modifier.padding(
                start = 24.dp,
                top = 24.dp,
                bottom = 16.dp
            )
        )
    }

    if (showLogout) { // 로그아웃 확인 팝업
        AlertDialog(
            onDismissRequest = {
                showLogout = false
            },
            containerColor = Color.White,
            text = {
                Text(
                    text = "로그아웃 하시겠습니까?",
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        showLogout = false
                        onLogout()
                    }
                ) {
                    Text("로그아웃")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        showLogout = false
                    }
                ) {
                    Text("취소")
                }
            }
        )
    }
}

/* 회원 정보의 항목 이름과 내용을 표시 */
@Composable
private fun InfoRow(
    label: String,
    value: String
) {
    Column(
        modifier = Modifier.padding(top = 12.dp)
    ) {
        Text(
            text = label,
            fontSize = 14.sp,
            color = Color.Gray
        )

        Text(
            text = value,
            fontSize = 18.sp,
            modifier = Modifier.padding(
                top = 2.dp,
                bottom = 10.dp
            )
        )

        HorizontalDivider()
    }
}