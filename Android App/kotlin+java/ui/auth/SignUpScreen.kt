package com.example.snapapp.ui.auth

import android.graphics.Bitmap
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Face
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.Icon
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.snapapp.ServerClient
import com.example.snapapp.toBase64
import com.example.snapapp.ui.FaceCaptureDialog
import com.example.snapapp.ui.LoadingDialog
import com.example.snapapp.ui.theme.SnapGreen
import kotlinx.coroutines.launch
import org.json.JSONArray
import org.json.JSONObject

/* 개인정보 수집,이용 동의 안내 문구 */
private const val INFO_TEXT =
    "SNAP은 회원가입, 서비스 제공 및 회원의 이용내역 관리를 위하여 「개인정보 보호법」 " +
            "제15조에 따라 다음과 같이 개인정보를 수집·이용합니다.\n\n" +
            "1. 수집·이용 목적: 회원가입 및 회원 관리, 서비스 이용자 식별, " +
            "잔액 충전 및 결제 처리, 충전·결제 이용내역 조회, 고객 문의 대응\n" +
            "2. 수집 항목: 이름, 아이디, 비밀번호, 휴대전화 번호, " +
            "충전·결제 내역(충전금액, 사용금액, 거래일시), 잔액 정보\n" +
            "3. 수집·이용 기간: 회원 탈퇴 시까지. 단, 관계 법령에 따라 " +
            "보존이 필요한 정보는 해당 법령에서 정한 기간 동안 보관합니다.\n" +
            "4. 동의 거부 권리 및 불이익: 회원은 개인정보 수집·이용에 대한 " +
            "동의를 거부할 수 있습니다. 다만, 필수 항목에 대한 동의를 " +
            "거부하는 경우 회원가입 또는 관련 서비스 이용이 제한될 수 있습니다.\n"

/* 민감정보 수집,이용 동의 안내 문구 */
private const val FACE_TEXT =
    "SNAP은 얼굴 인식을 통한 회원 식별 및 인증 서비스 제공을 위하여 「개인정보 보호법」 " +
            "제23조 등 관련 법령에 따라 다음과 같이 민감정보를 처리합니다.\n\n" +
            "1. 수집·이용 목적: 얼굴 인식을 통한 회원 식별 및 본인 인증, " +
            "키오스크 로그인, 얼굴 인증 기반 결제 서비스 제공\n" +
            "2. 수집 항목: 얼굴 이미지(원본정보) 및 얼굴 이미지로부터 추출하여 " +
            "회원 식별에 사용하는 얼굴 특징정보(생체인식정보)\n" +
            "3. 보유·이용 기간: 회원 탈퇴 또는 얼굴 인식 서비스 이용 종료 시까지. " +
            "단, 관계 법령에 따라 보존이 필요한 정보는 해당 법령에서 정한 기간 동안 보관합니다.\n" +
            "4. 동의 거부 권리 및 불이익: 회원은 민감정보 처리에 대한 동의를 거부할 수 있습니다. " +
            "동의하지 않을 경우 회원가입이 제한됩니다.\n"

@Composable
fun SignUpScreen(onDone: () -> Unit) {
    val scope = rememberCoroutineScope()

    /* 얼굴 정보 및 화면 상태 */
    var faceImages by remember { mutableStateOf<List<Bitmap>>(emptyList()) }
    var showFace by remember { mutableStateOf(false) }
    var showCamera by remember { mutableStateOf(false) }
    var isAdmin by remember { mutableStateOf(false) }
    var adminCode by remember { mutableStateOf("") }

    /* 회원 정보 입력 상태 */
    var name by remember { mutableStateOf("") }
    var id by remember { mutableStateOf("") }
    var idAvailable by remember { mutableStateOf<Boolean?>(null) }
    var pw by remember { mutableStateOf("") }
    var pw2 by remember { mutableStateOf("") }
    var phone by remember { mutableStateOf("") }

    /* 약관 동의 및 요청 상태 */
    var agreeInfo by remember { mutableStateOf(false) }
    var agreeFace by remember { mutableStateOf(false) }
    var loading by remember { mutableStateOf(false) }
    var message by remember { mutableStateOf<String?>(null) }
    var success by remember { mutableStateOf(false) }
    var consentText by remember { mutableStateOf<String?>(null) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .systemBarsPadding()
            .imePadding()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 24.dp)
    ) {
        /* 화면 제목 */
        Text(
            "회원가입",
            fontSize = 24.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 16.dp, bottom = 20.dp)
        )

        /* 얼굴 정보 등록 버튼 */
        OutlinedButton(
            onClick = { showFace = true },
            shape = RoundedCornerShape(12.dp),
            modifier = Modifier
                .fillMaxWidth()
                .height(48.dp)
        ) {
            Icon(
                Icons.Filled.Face,
                contentDescription = null
            )

            Spacer(modifier = Modifier.width(8.dp))

            Text(
                if (faceImages.isNotEmpty()) {
                    "얼굴 정보 등록 완료"
                } else {
                    "얼굴 정보 등록"
                }
            )
        }

        /* 관리자 가입 설정 */
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(top = 8.dp)
        ) {
            Checkbox(
                checked = isAdmin,
                onCheckedChange = {
                    isAdmin = it

                    if (!it) {
                        adminCode = ""
                    }
                }
            )

            Text("관리자로 가입")
        }

        InputBox(
            value = adminCode,
            onValueChange = { adminCode = it },
            placeholder = "관리자 코드",
            enabled = isAdmin,
            isPassword = true
        )

        /* 이름 입력 */
        Label("이름")

        InputBox(
            value = name,
            onValueChange = { name = it },
            placeholder = "이름"
        )

        /* 아이디 입력과 중복 확인 */
        Label("아이디")

        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            InputBox(
                value = id,
                onValueChange = {
                    id = it
                    idAvailable = null
                },
                placeholder = "아이디",
                modifier = Modifier.weight(1f)
            )

            Button(
                onClick = {
                    if (id.isBlank()) {
                        message = "아이디 입력"
                    } else {
                        scope.launch {
                            loading = true

                            try {
                                // 아이디 중복 확인 요청
                                val res = ServerClient.request(
                                    JSONObject()
                                        .put("cmd", "CHECK_ID")
                                        .put("loginId", id)
                                )

                                if (res.optBoolean("ok")) {
                                    idAvailable = res
                                        .optJSONObject("data")
                                        ?.optBoolean("available") == true
                                } else {
                                    message = res.optString(
                                        "error",
                                        "아이디 확인 실패"
                                    )
                                }
                            } catch (e: Exception) {
                                message = "서버에 연결할 수 없습니다."
                            }

                            loading = false
                        }
                    }
                },
                modifier = Modifier.height(56.dp)
            ) {
                Text("중복 확인")
            }
        }

        /* 아이디 중복 확인 결과 */
        idAvailable?.let {
            Text(
                if (it) {
                    "사용 가능한 아이디입니다."
                } else {
                    "이미 사용 중인 아이디입니다."
                },
                color = if (it) SnapGreen else Color.Red,
                fontSize = 13.sp,
                modifier = Modifier.padding(top = 4.dp)
            )
        }

        /* 비밀번호 입력 */
        Label("비밀번호")

        InputBox(
            value = pw,
            onValueChange = { pw = it },
            placeholder = "**********",
            isPassword = true
        )

        /* 비밀번호 확인 */
        Label("비밀번호 재입력")

        InputBox(
            value = pw2,
            onValueChange = { pw2 = it },
            placeholder = "**********",
            isPassword = true
        )

        if (pw2.isNotEmpty() && pw != pw2) {
            Text(
                "비밀번호가 일치하지 않습니다.",
                color = Color.Red,
                fontSize = 13.sp,
                modifier = Modifier.padding(top = 4.dp)
            )
        }

        /* 휴대전화 번호 입력 */
        Label("휴대전화 번호")

        InputBox(
            value = phone,
            onValueChange = { phone = it },
            placeholder = "010-0000-0000",
            keyboardType = KeyboardType.Phone
        )

        /* 필수 약관 동의 */
        ConsentRow(
            text = "개인정보 수집·이용 동의(필수)",
            checked = agreeInfo,
            onCheckedChange = { agreeInfo = it },
            onDetail = { consentText = INFO_TEXT }
        )

        ConsentRow(
            text = "민감정보 수집·이용 동의(필수)",
            checked = agreeFace,
            onCheckedChange = { agreeFace = it },
            onDetail = { consentText = FACE_TEXT }
        )

        /* 회원가입 요청 */
        Button(
            onClick = {
                val problem = when {
                    faceImages.isEmpty() ->
                        "얼굴 정보를 등록해 주세요."

                    isAdmin && adminCode.isBlank() ->
                        "관리자 코드를 입력해 주세요."

                    name.isBlank() ->
                        "이름을 입력해 주세요."

                    idAvailable != true ->
                        "아이디 중복을 확인해 주세요."

                    pw.isBlank() || pw != pw2 ->
                        "비밀번호를 확인해 주세요."

                    phone.isBlank() ->
                        "휴대전화 번호를 입력해 주세요."

                    else -> null
                }

                if (problem != null) {
                    message = problem
                } else {
                    scope.launch {
                        loading = true

                        try {
                            /* 입력한 회원 정보와 얼굴 이미지를 서버로 전송 */
                            val res = ServerClient.request(
                                JSONObject()
                                    .put("cmd", "REGISTER")
                                    .put("name", name)
                                    .put("loginId", id)
                                    .put("password", pw)
                                    .put("phone", phone.filter { it.isDigit() })
                                    .put(
                                        "adminCode",
                                        if (isAdmin) adminCode else ""
                                    )
                                    .put(
                                        "faces",
                                        JSONArray(
                                            faceImages.map { it.toBase64() }
                                        )
                                    )
                            )

                            if (res.optBoolean("ok")) {
                                success = true
                                message = "회원가입이 완료되었습니다."
                            } else {
                                message = res.optString(
                                    "error",
                                    "회원가입에 실패했습니다."
                                )
                            }
                        } catch (e: Exception) {
                            message = "서버에 연결할 수 없습니다."
                        }

                        loading = false
                    }
                }
            },
            enabled = agreeInfo && agreeFace,
            modifier = Modifier
                .align(Alignment.CenterHorizontally)
                .padding(vertical = 20.dp)
                .width(140.dp)
                .height(44.dp)
        ) {
            Text("가입하기")
        }
    }

    if (showFace) { // 얼굴 정보 등록에 따른 안내 팝업
        AlertDialog(
            onDismissRequest = { showFace = false },
            containerColor = Color.White,
            icon = {
                Icon(
                    Icons.Filled.Face,
                    contentDescription = null,
                    modifier = Modifier.size(64.dp)
                )
            },
            title = {
                Text("얼굴 정보 등록")
            },
            text = {
                Text(
                    "밝은 곳에서 얼굴이 잘 보이도록\n각도를 조금씩 바꿔 5장 촬영해 주세요",
                    textAlign = TextAlign.Center,
                    modifier = Modifier.fillMaxWidth()
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        showFace = false
                        showCamera = true
                    }
                ) {
                    Text("사진 촬영")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = { showFace = false }
                ) {
                    Text("취소")
                }
            }
        )
    }

    /* 얼굴 촬영 화면 */
    if (showCamera) {
        FaceCaptureDialog(
            onDismiss = { showCamera = false },
            onCaptured = {
                faceImages = it
                showCamera = false
            }
        )
    }

    /* 서버 요청 중 로딩 표시 */
    if (loading) {
        LoadingDialog()
    }

    message?.let { // 요청 결과 메시지 팝업
        AlertDialog(
            onDismissRequest = { },
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
                    onClick = {
                        message = null

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

    /* 개인정보, 민감정보 동의 상세 내용 */
    consentText?.let {
        AlertDialog(
            onDismissRequest = { consentText = null },
            containerColor = Color.White,
            text = {
                Text(
                    it,
                    fontSize = 14.sp
                )
            },
            confirmButton = {
                TextButton(
                    onClick = { consentText = null }
                ) {
                    Text("닫기")
                }
            }
        )
    }
}

/* 입력 항목 제목 */
@Composable
private fun Label(text: String) {
    Text(
        text,
        fontSize = 15.sp,
        fontWeight = FontWeight.Medium,
        modifier = Modifier.padding(
            top = 16.dp,
            bottom = 8.dp
        )
    )
}

/* 공통 입력 필드 */
@Composable
private fun InputBox(
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    modifier: Modifier = Modifier.fillMaxWidth(),
    enabled: Boolean = true,
    isPassword: Boolean = false,
    keyboardType: KeyboardType = KeyboardType.Text
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier,
        enabled = enabled,
        placeholder = { Text(placeholder) },
        singleLine = true,
        shape = RoundedCornerShape(12.dp),
        visualTransformation = if (isPassword) {
            PasswordVisualTransformation()
        } else {
            VisualTransformation.None
        },
        keyboardOptions = KeyboardOptions(
            keyboardType = if (isPassword) {
                KeyboardType.Password
            } else {
                keyboardType
            }
        )
    )
}

/* 약관 동의 항목과 상세 보기 버튼 */
@Composable
private fun ConsentRow(
    text: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    onDetail: () -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier.padding(top = 8.dp)
    ) {
        Checkbox(
            checked = checked,
            onCheckedChange = onCheckedChange
        )

        Text(
            text,
            fontSize = 13.sp,
            modifier = Modifier.weight(1f)
        )

        TextButton(
            onClick = onDetail
        ) {
            Text(
                "보기 >",
                fontSize = 13.sp
            )
        }
    }
}