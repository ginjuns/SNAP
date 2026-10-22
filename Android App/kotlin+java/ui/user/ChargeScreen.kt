package com.example.snapapp.ui.user

import android.app.Activity
import android.content.Context
import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
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
import com.example.snapapp.ui.theme.SnapGreen
import kotlinx.coroutines.launch
import org.json.JSONObject

@Composable
fun ChargeScreen(onDone: () -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    /* 로그인 정보 가져오기 */
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

    /* 충전 및 결제 상태 */
    var balance by remember { mutableStateOf<Int?>(null) }
    var amount by remember { mutableIntStateOf(0) }
    var charged by remember { mutableStateOf<Int?>(null) }

    /* 비밀번호 확인 상태 */
    var showPw by remember { mutableStateOf(false) }
    var pwInput by remember { mutableStateOf("") }
    var confirmedPw by remember { mutableStateOf("") }

    /* 요청 및 오류 상태 */
    var loading by remember { mutableStateOf(false) }
    var errorMessage by remember { mutableStateOf<String?>(null) }

    /* 화면 진입 시 현재 잔액 조회 */
    LaunchedEffect(Unit) {
        try {
            val res = ServerClient.request(
                JSONObject()
                    .put("cmd", "LOGIN")
                    .put("userId", userNo)
            )

            if (res.optBoolean("ok")) {
                balance = res.getJSONObject("data").optInt("balance")
            }
        } catch (e: Exception) {
            // 연결 실패 시 잔액을 "- 원"으로 표시
        }
    }

    /* 토스 결제 결과 처리 */
    val paymentLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK) {
            // 결제 성공 후 서버에 최종 승인, 충전 요청
            scope.launch {
                loading = true

                try {
                    val res = ServerClient.request(
                        JSONObject()
                            .put("cmd", "CHARGE")
                            .put("loginId", loginId)
                            .put("password", confirmedPw)
                            .put("amount", amount)
                            .put(
                                "paymentKey",
                                result.data?.getStringExtra("paymentKey")
                            )
                            .put(
                                "orderId",
                                result.data?.getStringExtra("orderId")
                            ),
                        timeoutMs = 30000
                    )

                    if (res.optBoolean("ok")) {
                        balance = res.getJSONObject("data").optInt("balance")
                        charged = amount
                    } else {
                        errorMessage = res.optString(
                            "error",
                            "충전에 실패했습니다."
                        )
                    }
                } catch (e: Exception) {
                    errorMessage =
                        "서버에 연결할 수 없습니다.\n결제가 완료되지 않았다면 잠시 후 자동 취소됩니다."
                }

                loading = false
            }
        } else {
            // 결제 취소 또는 실패
            errorMessage = result.data?.getStringExtra("message")
                ?: "결제가 취소되었습니다."
        }
    }

    val done = charged // 충전 완료 시 완료 화면 표시

    if (done != null) {
        ChargeCompleteContent(
            charged = done,
            balanceAfter = balance ?: 0,
            onConfirm = onDone
        )
        return
    }

    /* 충전 화면 */
    Column(
        modifier = Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .padding(horizontal = 24.dp)
    ) {
        /* 화면 제목 */
        Text(
            "스냅머니 충전",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(
                    top = 16.dp,
                    bottom = 32.dp
                )
        )

        /* 현재 보유 잔액 */
        Text(
            "보유 잔액",
            fontSize = 15.sp,
            color = Color.Gray
        )

        Text(
            balance?.let { "%,d 원".format(it) } ?: "- 원",
            fontSize = 28.sp,
            fontWeight = FontWeight.Bold
        )

        /* 충전 금액 선택 영역 */
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 24.dp)
        ) {
            Text(
                "충전 금액",
                fontSize = 15.sp,
                color = Color.Gray,
                modifier = Modifier.weight(1f)
            )

            TextButton(
                onClick = { amount = 0 }
            ) {
                Text(
                    "초기화",
                    color = Color.Gray
                )
            }
        }

        /* 선택한 충전 금액 */
        OutlinedTextField(
            value = "%,d 원".format(amount),
            onValueChange = { },
            readOnly = true,
            singleLine = true,
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier.fillMaxWidth()
        )

        /* 충전 금액 추가 버튼 */
        Row(
            horizontalArrangement = Arrangement.spacedBy(12.dp),
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 12.dp)
        ) {
            listOf(10000, 30000, 50000).forEach { add ->
                OutlinedButton(
                    onClick = { amount += add },
                    border = BorderStroke(1.dp, SnapGreen),
                    shape = RoundedCornerShape(12.dp),
                    contentPadding = PaddingValues(horizontal = 4.dp),
                    modifier = Modifier.weight(1f)
                ) {
                    Text(
                        "+%,d".format(add),
                        color = SnapGreen,
                        maxLines = 1
                    )
                }
            }
        }

        /* 최종 결제 금액 */
        HorizontalDivider(
            modifier = Modifier.padding(top = 32.dp)
        )

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 16.dp)
        ) {
            Text(
                "최종 결제 금액",
                fontSize = 16.sp,
                color = Color.Gray,
                modifier = Modifier.weight(1f)
            )

            Text(
                "%,d 원".format(amount),
                fontSize = 16.sp,
                color = Color.Gray
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        /* 결제 진행 버튼 */
        Button(
            onClick = {
                pwInput = ""
                showPw = true
            },
            enabled = amount > 0,
            modifier = Modifier
                .align(Alignment.CenterHorizontally)
                .padding(bottom = 24.dp)
                .width(140.dp)
                .height(44.dp)
        ) {
            Text("결제하기")
        }
    }

    if (showPw) { // 결제 전 비밀번호 확인 팝업
        AlertDialog(
            onDismissRequest = { showPw = false },
            containerColor = Color.White,
            title = {
                Text("비밀번호 확인")
            },
            text = {
                OutlinedTextField(
                    value = pwInput,
                    onValueChange = { pwInput = it },
                    placeholder = { Text("비밀번호") },
                    singleLine = true,
                    visualTransformation = PasswordVisualTransformation(),
                    keyboardOptions = KeyboardOptions(
                        keyboardType = KeyboardType.Password
                    ),
                    shape = RoundedCornerShape(12.dp)
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        showPw = false

                        scope.launch {
                            loading = true

                            try {
                                /* 입력한 비밀번호 검증 */
                                val res = ServerClient.request(
                                    JSONObject()
                                        .put("cmd", "PASSWORD_LOGIN")
                                        .put("loginId", loginId)
                                        .put("password", pwInput)
                                )

                                if (res.optBoolean("ok")) {
                                    confirmedPw = pwInput
                                    loading = false

                                    /* 비밀번호 확인 성공 시 토스 결제 화면 실행 */
                                    val intent = Intent(
                                        context,
                                        TossPaymentActivity::class.java
                                    ).putExtra("amount", amount)

                                    paymentLauncher.launch(intent)
                                } else {
                                    errorMessage = "비밀번호가 올바르지 않습니다."
                                }
                            } catch (e: Exception) {
                                errorMessage = "서버에 연결할 수 없습니다."
                            }

                            loading = false
                        }
                    }
                ) {
                    Text("확인")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = { showPw = false }
                ) {
                    Text("취소")
                }
            }
        )
    }

    if (loading) { // 로딩 창
        LoadingDialog()
    }

    errorMessage?.let { // 오류 메시지 팝업
        AlertDialog(
            onDismissRequest = { errorMessage = null },
            containerColor = Color.White,
            text = {
                Text(
                    it,
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                Button(
                    onClick = { errorMessage = null }
                ) {
                    Text("확인")
                }
            }
        )
    }
}

/* 충전 완료 화면 */
@Composable
private fun ChargeCompleteContent(
    charged: Int,
    balanceAfter: Int,
    onConfirm: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .padding(horizontal = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Spacer(modifier = Modifier.weight(1f))

        /* 충전 성공 아이콘 */
        Icon(
            Icons.Filled.CheckCircle,
            contentDescription = null,
            tint = SnapGreen,
            modifier = Modifier.size(72.dp)
        )

        Text(
            "충전이 완료되었습니다.",
            fontSize = 18.sp,
            modifier = Modifier.padding(top = 24.dp)
        )

        /* 충전 금액 표시 */
        Text(
            "충전 금액",
            fontSize = 15.sp,
            color = Color.Gray,
            modifier = Modifier.padding(top = 16.dp)
        )

        Text(
            "+ %,d".format(charged),
            fontSize = 22.sp,
            fontWeight = FontWeight.Bold
        )

        /* 충전 후 잔액 표시 */
        HorizontalDivider(
            modifier = Modifier.padding(top = 48.dp)
        )

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 16.dp)
        ) {
            Text(
                "충전 후 잔액",
                fontSize = 16.sp,
                color = Color.Gray,
                modifier = Modifier.weight(1f)
            )

            Text(
                "%,d 원".format(balanceAfter),
                fontSize = 16.sp,
                color = Color.Gray
            )
        }

        Spacer(modifier = Modifier.weight(1f))

        /* 완료 버튼 */
        Button(
            onClick = onConfirm,
            modifier = Modifier
                .padding(bottom = 24.dp)
                .width(140.dp)
                .height(44.dp)
        ) {
            Text("확인")
        }
    }
}