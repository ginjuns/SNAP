package com.example.snapapp.ui.user

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowLeft
import androidx.compose.material.icons.automirrored.filled.KeyboardArrowRight
import androidx.compose.material.icons.filled.DateRange
import androidx.compose.material3.DatePicker
import androidx.compose.material3.DatePickerDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberDatePickerState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.snapapp.ServerClient
import com.example.snapapp.ui.theme.SnapGreen
import org.json.JSONObject
import java.time.Instant
import java.time.LocalDate
import java.time.LocalDateTime
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter

// method: "card", "face"
private data class HistoryItem(
    val time: LocalDateTime,
    val isCharge: Boolean,
    val method: String,
    val amount: Int
)

/* 서버에서 전달하는 날짜 및 시간 형식 */
private val serverTime =
    DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss")

private val serverDate =
    DateTimeFormatter.ofPattern("yyyy-MM-dd")

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HistoryScreen() {
    val context = LocalContext.current

    /* 로그인한 사용자 번호 가져오기 */
    val userNo = remember {
        context.getSharedPreferences(
            "loginPrefs",
            Context.MODE_PRIVATE
        ).getInt("userNo", 0)
    }

    /* 일간, 월간 조회 상태 */
    var monthly by remember {
        mutableStateOf(false)
    }

    /* 조회할 날짜 */
    var date by remember {
        mutableStateOf(LocalDate.now())
    }

    /* 날짜 선택 달력 표시 여부 */
    var showPicker by remember {
        mutableStateOf(false)
    }

    /* 서버에서 받아온 이용 내역 */
    var items by remember {
        mutableStateOf<List<HistoryItem>>(emptyList())
    }

    /* 오류 메시지 */
    var errorText by remember {
        mutableStateOf<String?>(null)
    }

    /* 날짜 또는 조회 유형 바뀌면 서버에서 내역 조회 */
    LaunchedEffect(date, monthly) {
        val from = if (monthly) {
            date.withDayOfMonth(1)
        } else {
            date
        }

        val to = if (monthly) {
            from.plusMonths(1)
        } else {
            from.plusDays(1)
        }

        try {
            val res = ServerClient.request(
                JSONObject()
                    .put("cmd", "HISTORY")
                    .put("userId", userNo)
                    .put("from", from.format(serverDate))
                    .put("to", to.format(serverDate))
            )

            if (res.optBoolean("ok")) {
                val list = res.getJSONArray("data")

                items = (0 until list.length()).map { i ->
                    val row = list.getJSONObject(i)

                    HistoryItem(
                        time = LocalDateTime.parse(
                            row.optString("time"),
                            serverTime
                        ),
                        isCharge = row.optString("type") == "charge",
                        method = row.optString("method"),
                        amount = row.optInt("amount")
                    )
                }

                errorText = null
            } else {
                items = emptyList()
                errorText = res.optString(
                    "error",
                    "이용 내역을 불러오지 못했습니다."
                )
            }
        } catch (e: Exception) {
            items = emptyList()
            errorText = "서버에 연결할 수 없습니다."
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = 24.dp)
    ) {
        /* 화면 제목 */
        Text(
            text = "이용내역",
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

        /* 일간, 월간 조회 선택 */
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .border(
                    width = 1.dp,
                    color = Color(0xFFE0E0E0),
                    shape = RoundedCornerShape(12.dp)
                )
                .padding(4.dp)
        ) {
            listOf("일간", "월간").forEachIndexed { index, label ->
                val selected = monthly == (index == 1)

                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .weight(1f)
                        .clip(RoundedCornerShape(8.dp))
                        .background(
                            if (selected) {
                                SnapGreen
                            } else {
                                Color.Transparent
                            }
                        )
                        .clickable {
                            monthly = index == 1
                        }
                        .padding(vertical = 8.dp)
                ) {
                    Text(
                        text = label,
                        color = if (selected) {
                            Color.White
                        } else {
                            SnapGreen
                        }
                    )
                }
            }
        }

        /* 날짜 이동 및 달력 선택 */
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 12.dp)
        ) {
            /* 이전 날짜 또는 이전 달 */
            IconButton(
                onClick = {
                    date = if (monthly) {
                        date.minusMonths(1)
                    } else {
                        date.minusDays(1)
                    }
                }
            ) {
                Icon(
                    imageVector = Icons.AutoMirrored.Filled.KeyboardArrowLeft,
                    contentDescription = "이전"
                )
            }

            /* 현재 조회 날짜 표시 */
            Text(
                text = date.format(
                    DateTimeFormatter.ofPattern(
                        if (monthly) "yyyy.MM" else "yyyy.MM.dd"
                    )
                ),
                fontSize = 17.sp,
                fontWeight = FontWeight.Medium
            )

            /* 달력 열기 */
            IconButton(
                onClick = {
                    showPicker = true
                }
            ) {
                Icon(
                    imageVector = Icons.Filled.DateRange,
                    contentDescription = "달력"
                )
            }

            /* 다음 날짜 또는 다음 달 */
            IconButton(
                onClick = {
                    date = if (monthly) {
                        date.plusMonths(1)
                    } else {
                        date.plusDays(1)
                    }
                }
            ) {
                Icon(
                    imageVector = Icons.AutoMirrored.Filled.KeyboardArrowRight,
                    contentDescription = "다음"
                )
            }
        }

        /* 이용 내역이 없거나 조회에 실패한 경우 */
        if (items.isEmpty()) {
            Text(
                text = errorText ?: "이용 내역이 없습니다.",
                color = Color.Gray,
                textAlign = TextAlign.Center,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 48.dp)
            )
        } else {
            /* 이용 내역 목록 */
            LazyColumn {
                items(items) { item ->
                    Column(
                        modifier = Modifier
                            .padding(vertical = 10.dp)
                    ) {
                        /* 이용 날짜 및 시간 */
                        Text(
                            text = item.time.format(
                                DateTimeFormatter.ofPattern(
                                    "yyyy.MM.dd HH:mm"
                                )
                            ),
                            fontSize = 13.sp,
                            color = Color.Gray
                        )

                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(top = 4.dp)
                        ) {
                            /* 이용 유형 표시 */
                            Text(
                                text = when {
                                    item.isCharge -> "충전"
                                    item.method == "card" -> "카드 결제"
                                    else -> "얼굴인식 결제"
                                },
                                fontSize = 18.sp,
                                modifier = Modifier.weight(1f)
                            )

                            /* 충전은 '+금액', 결제는 '-금액'으로 표시 */
                            Text(
                                text = if (item.isCharge) {
                                    "+%,d 원".format(item.amount)
                                } else {
                                    "-%,d 원".format(item.amount)
                                },
                                fontSize = 18.sp,
                                fontWeight = FontWeight.Medium,
                                color = if (item.isCharge) {
                                    SnapGreen
                                } else {
                                    Color.Unspecified
                                }
                            )
                        }
                    }

                    HorizontalDivider()
                }
            }
        }
    }

    /* 날짜 선택 달력 */
    if (showPicker) {
        val pickerState = rememberDatePickerState(
            initialSelectedDateMillis = date
                .atStartOfDay(ZoneOffset.UTC)
                .toInstant()
                .toEpochMilli()
        )

        DatePickerDialog(
            onDismissRequest = {
                showPicker = false
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        pickerState.selectedDateMillis?.let {
                            date = Instant.ofEpochMilli(it)
                                .atZone(ZoneOffset.UTC)
                                .toLocalDate()
                        }

                        showPicker = false
                    }
                ) {
                    Text("확인")
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        showPicker = false
                    }
                ) {
                    Text("취소")
                }
            }
        ) {
            DatePicker(state = pickerState)
        }
    }
}