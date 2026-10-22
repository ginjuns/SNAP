package com.example.snapapp.ui.user

import android.content.Context
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.snapapp.ServerClient
import com.example.snapapp.ui.LoadingDialog
import com.example.snapapp.ui.theme.SnapGreen
import kotlinx.coroutines.launch
import org.json.JSONObject

@Composable
fun PasswordChangeScreen(
    onDone: () -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    /* 로그인한 사용자 아이디 가져오기 */
    val loginId = remember {
        context.getSharedPreferences(
            "loginPrefs",
            Context.MODE_PRIVATE
        ).getString("userId", "") ?: ""
    }

    /* 비밀번호 입력 상태 */
    var current by remember {
        mutableStateOf("")
    }

    var newPw by remember {
        mutableStateOf("")
    }

    var newPw2 by remember {
        mutableStateOf("")
    }

    /* 로딩 및 결과 메시지 상태 */
    var loading by remember {
        mutableStateOf(false)
    }

    var message by remember {
        mutableStateOf<String?>(null)
    }

    var success by remember {
        mutableStateOf(false)
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .imePadding()
            .padding(horizontal = 24.dp)
    ) {
        /* 화면 제목 */
        Text(
            text = "비밀번호 변경",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(
                    top = 16.dp,
                    bottom = 24.dp
                )
        )

        PwField( // 현재 비밀번호 입력
            label = "현재 비밀번호",
            value = current,
            onValueChange = { current = it }
        )

        PwField( // 새 비밀번호 입력
            label = "새 비밀번호 입력",
            value = newPw,
            onValueChange = { newPw = it }
        )

        PwField( // 새 비밀번호 확인
            label = "새 비밀번호 확인",
            value = newPw2,
            onValueChange = { newPw2 = it }
        )

        if (newPw2.isNotEmpty() && newPw != newPw2) { // 새 비밀번호 일치 여부 확인
            Text(
                text = "비밀번호가 일치하지 않습니다.",
                color = Color.Red,
                fontSize = 13.sp,
                modifier = Modifier.padding(top = 4.dp)
            )
        }

        Spacer(
            modifier = Modifier.weight(1f)
        )

        /* 취소 및 변경 버튼 */
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 24.dp)
        ) {
            OutlinedButton(
                onClick = onDone,
                border = BorderStroke(
                    1.dp,
                    SnapGreen
                ),
                modifier = Modifier
                    .weight(1f)
                    .height(44.dp)
            ) {
                Text(
                    text = "취소",
                    color = SnapGreen
                )
            }

            Button(
                onClick = {
                    // 입력값 검증
                    if (current.isBlank()) {
                        message = "현재 비밀번호를 입력해 주세요."
                    } else if (
                        newPw.isBlank() || newPw != newPw2
                    ) {
                        message = "새 비밀번호를 확인해 주세요."
                    } else {
                        // 서버에 비밀번호 변경 요청
                        scope.launch {
                            loading = true

                            try {
                                val res = ServerClient.request(
                                    JSONObject()
                                        .put("cmd", "CHANGE_ACCOUNT")
                                        .put("loginId", loginId)
                                        .put("password", current)
                                        .put("newPassword", newPw)
                                )

                                if (res.optBoolean("ok")) {
                                    success = true
                                    message = "변경되었습니다."
                                } else {
                                    message = res.optString(
                                        "error",
                                        "현재 비밀번호가 일치하지 않습니다."
                                    )
                                }
                            } catch (e: Exception) {
                                message = "서버에 연결할 수 없습니다."
                            }

                            loading = false
                        }
                    }
                },
                modifier = Modifier
                    .weight(1f)
                    .height(44.dp)
            ) {
                Text("변경")
            }
        }
    }

    if (loading) { // 로딩 창
        LoadingDialog()
    }

    message?.let { // 결과 메시지 팝업
        AlertDialog(
            onDismissRequest = {
                // 확인 버튼으로 닫도록 유지
            },
            containerColor = Color.White,
            text = {
                Text(
                    text = it,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        message = null

                        // 변경 성공 시 이전 화면으로 이동
                        if (success) {
                            onDone()
                        }
                    }
                ) {
                    Text("확인")
                }
            }
        )
    }
}

/* 비밀번호 입력 필드 */
@Composable
private fun PwField(
    label: String,
    value: String,
    onValueChange: (String) -> Unit
) {
    Column(
        modifier = Modifier.padding(top = 16.dp)
    ) {
        Text(
            text = label,
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium,
            modifier = Modifier.padding(bottom = 8.dp)
        )

        OutlinedTextField(
            value = value,
            onValueChange = onValueChange,
            placeholder = {
                Text("**********")
            },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(
                keyboardType = KeyboardType.Password
            ),
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier.fillMaxWidth()
        )
    }
}