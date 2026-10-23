package com.example.snapapp.ui.auth

import androidx.compose.foundation.Image
import androidx.compose.ui.res.painterResource
import com.example.snapapp.R
import android.content.Context
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
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
import com.example.snapapp.ui.theme.SnapBarBg
import com.example.snapapp.ui.theme.SnapGreen
import kotlinx.coroutines.launch
import org.json.JSONObject

@Composable
fun LoginScreen(
    onLoginSuccess: (role: String) -> Unit,
    onSignUp: () -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    /* 입력값 및 화면 상태 */
    var id by remember { mutableStateOf("") }
    var pw by remember { mutableStateOf("") }
    var autoLogin by remember { mutableStateOf(false) }
    var loading by remember { mutableStateOf(false) }
    var errorText by remember { mutableStateOf<String?>(null) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .padding(horizontal = 12.dp)
    ) {
        Spacer(modifier = Modifier.height(20.dp))

        Image(
            painter = painterResource(R.drawable.logo),
            contentDescription = "SNAP",
            modifier = Modifier
                .align(Alignment.CenterHorizontally)
                .padding(bottom = 5.dp)
                .size(200.dp)
        )

        Text( // 아이디 입력
            "ID",
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium
        )

        OutlinedTextField(
            value = id,
            onValueChange = { id = it },
            placeholder = { Text("ID") },
            singleLine = true,
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 8.dp)
        )

        Text( // 비밀번호 입력
            "Password",
            fontSize = 15.sp,
            fontWeight = FontWeight.Medium,
            modifier = Modifier.padding(top = 20.dp)
        )

        OutlinedTextField(
            value = pw,
            onValueChange = { pw = it },
            placeholder = { Text("**********") },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            keyboardOptions = KeyboardOptions(
                keyboardType = KeyboardType.Password
            ),
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 8.dp)
        )

        /* 자동 로그인 설정 */
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(top = 8.dp)
        ) {
            Checkbox(
                checked = autoLogin,
                onCheckedChange = { autoLogin = it }
            )

            Text(
                "자동 로그인",
                fontSize = 13.sp
            )
        }

        /* 회원가입 및 로그인 버튼 */
        Row(
            horizontalArrangement = Arrangement.spacedBy(16.dp),
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 4.dp)
        ) {
            Button(
                onClick = onSignUp,
                colors = ButtonDefaults.buttonColors(
                    containerColor = SnapBarBg,
                    contentColor = SnapGreen
                ),
                modifier = Modifier
                    .weight(1f)
                    .height(44.dp)
            ) {
                Text("회원가입")
            }

            Button(
                onClick = {
                    scope.launch {
                        loading = true

                        try {
                            /* 로그인 요청 */
                            val res = ServerClient.request(
                                JSONObject()
                                    .put("cmd", "PASSWORD_LOGIN")
                                    .put("loginId", id)
                                    .put("password", pw)
                            )

                            if (res.optBoolean("ok")) {
                                /* 로그인 성공 시 사용자 정보 저장 */
                                val data = res.getJSONObject("data")
                                val role = if (
                                    data.optString("role") == "admin"
                                ) {
                                    "ADMIN"
                                } else {
                                    "USER"
                                }

                                context.getSharedPreferences(
                                    "loginPrefs",
                                    Context.MODE_PRIVATE
                                )
                                    .edit()
                                    .putBoolean("autoLogin", autoLogin)
                                    .putString("userId", id)
                                    .putInt("userNo", data.optInt("id"))
                                    .putString("role", role)
                                    .putString("token", data.optString("token"))
                                    .apply()

                                /* 서버 인증 토큰 설정 */
                                ServerClient.token = data.optString("token")

                                loading = false
                                onLoginSuccess(role)
                            } else {
                                /* 로그인 실패 메시지 표시 */
                                loading = false
                                errorText = res.optString(
                                    "error",
                                    "아이디 또는 비밀번호가 올바르지 않습니다."
                                )
                            }
                        } catch (e: Exception) {
                            /* 서버 연결 오류 처리 */
                            loading = false
                            errorText = "서버에 연결할 수 없습니다."
                        }
                    }
                },
                modifier = Modifier
                    .weight(1f)
                    .height(44.dp)
            ) {
                Text("로그인")
            }
        }

        Text( // 비밀번호 재설정 안내
            "비밀번호를 잊으셨나요?\n비밀번호 재설정은 고객센터를 문의해 주세요.",
            fontSize = 13.sp,
            modifier = Modifier.padding(top = 28.dp)
        )

        Spacer(modifier = Modifier.weight(1f))

        Text( // 고객센터 연락처
            "고객센터: 1234-5678",
            fontSize = 12.sp,
            color = Color.Gray,
            textAlign = TextAlign.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(bottom = 16.dp)
        )
    }

    if (loading) { // 로딩 표시
        LoadingDialog()
    }

    errorText?.let { // 오류 발생할 경우 안내 팝업
        AlertDialog(
            onDismissRequest = { errorText = null },
            containerColor = Color.White,
            icon = {
                Icon(
                    Icons.Outlined.Warning,
                    contentDescription = null,
                    modifier = Modifier.size(48.dp)
                )
            },
            text = {
                Text(
                    it,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                Button(
                    onClick = { errorText = null }
                ) {
                    Text("확인")
                }
            }
        )
    }
}