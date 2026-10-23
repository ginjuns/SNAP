package com.example.snapapp.ui.admin

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
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
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.snapapp.ServerClient
import com.example.snapapp.ui.theme.SnapGreen
import org.json.JSONObject
import java.time.Instant
import java.time.LocalDate
import java.time.ZoneOffset
import java.time.format.DateTimeFormatter

private val CardRed = Color(0xFFC96F70)
private val Faceblue = Color(0xFF648BB5)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SalesScreen() {
    /* 화면 상태 */
    var monthly by remember { mutableStateOf(false) }
    var date by remember { mutableStateOf(LocalDate.now()) }
    var showPicker by remember { mutableStateOf(false) }

    /* 결제 방식별 판매량 */
    var faceByBucket by remember { mutableStateOf<Map<Int, Int>>(emptyMap()) }
    var cardByBucket by remember { mutableStateOf<Map<Int, Int>>(emptyMap()) }

    /* 상품별 판매 내역 */
    var byProduct by remember {
        mutableStateOf<List<Triple<String, Int, Int>>>(emptyList())
    }

    /* 오류 메시지 */
    var errorText by remember { mutableStateOf<String?>(null) }

    /* 날짜나 일간, 월간이 바뀔 때마다 서버에서 판매량 받아오기 */
    LaunchedEffect(date, monthly) {
        try {
            val res = ServerClient.request(
                JSONObject()
                    .put("cmd", "SALES")
                    .put("unit", if (monthly) "month" else "day")
                    .put(
                        "date",
                        date.format(
                            DateTimeFormatter.ofPattern(
                                if (monthly) "yyyy-MM" else "yyyy-MM-dd"
                            )
                        )
                    )
            )

            if (res.optBoolean("ok")) {
                val data = res.getJSONObject("data")

                /* 시간대, 날짜별 결제 방식 판매량 */
                val buckets = data.getJSONArray("buckets")
                val face = mutableMapOf<Int, Int>()
                val card = mutableMapOf<Int, Int>()

                for (i in 0 until buckets.length()) {
                    val b = buckets.getJSONObject(i)

                    face[b.optInt("bucket")] = b.optInt("face")
                    card[b.optInt("bucket")] = b.optInt("card")
                }

                faceByBucket = face
                cardByBucket = card

                /* 상품별 판매량 집계 */
                val details = data.getJSONArray("details")
                val rows = (0 until details.length()).map {
                    details.getJSONObject(it)
                }

                byProduct = rows
                    .groupBy { it.optString("product") }
                    .map { (name, list) ->
                        Triple(
                            name,
                            list.sumOf { it.optInt("qty") },
                            list.sumOf { it.optInt("amount") }
                        )
                    }
                    .sortedByDescending { it.third }

                errorText = null
            } else {
                faceByBucket = emptyMap()
                cardByBucket = emptyMap()
                byProduct = emptyList()
                errorText = res.optString(
                    "error",
                    "판매량을 불러오지 못했습니다."
                )
            }
        } catch (e: Exception) {
            faceByBucket = emptyMap()
            cardByBucket = emptyMap()
            byProduct = emptyList()
            errorText = "서버에 연결할 수 없습니다."
        }
    }

    /* 판매량 그래프 데이터 */
    val buckets = if (monthly) {
        (1..date.lengthOfMonth()).toList()
    } else {
        (0..23).toList()
    }

    val faceTotal = faceByBucket.values.sum()
    val cardTotal = cardByBucket.values.sum()
    val total = faceTotal + cardTotal

    Column(
        modifier = Modifier
            .fillMaxSize()
            .statusBarsPadding()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 24.dp)
    ) {
        /* 화면 제목 */
        Text(
            "판매량",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 16.dp, bottom = 24.dp)
        )

        /* 일간, 월간 선택 */
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .border(
                    1.dp,
                    Color(0xFFE0E0E0),
                    RoundedCornerShape(12.dp)
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
                            if (selected) SnapGreen else Color.Transparent
                        )
                        .clickable {
                            monthly = index == 1
                        }
                        .padding(vertical = 8.dp)
                ) {
                    Text(
                        label,
                        color = if (selected) Color.White else SnapGreen
                    )
                }
            }
        }

        /* 날짜 선택 */
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 12.dp)
        ) {
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
                    Icons.AutoMirrored.Filled.KeyboardArrowLeft,
                    contentDescription = "이전"
                )
            }

            Text(
                date.format(
                    DateTimeFormatter.ofPattern(
                        if (monthly) "yyyy.MM" else "yyyy.MM.dd"
                    )
                ),
                fontSize = 17.sp,
                fontWeight = FontWeight.Medium
            )

            IconButton(
                onClick = {
                    showPicker = true
                }
            ) {
                Icon(
                    Icons.Filled.DateRange,
                    contentDescription = "달력"
                )
            }

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
                    Icons.AutoMirrored.Filled.KeyboardArrowRight,
                    contentDescription = "다음"
                )
            }
        }

        /* 매출 요약 */
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp)
        ) {
            SummaryItem(
                "총 매출",
                total,
                Modifier.weight(1f)
            )

            SummaryItem(
                "얼굴 인식",
                faceTotal,
                Modifier.weight(1f)
            )

            SummaryItem(
                "카드",
                cardTotal,
                Modifier.weight(1f)
            )
        }

        HorizontalDivider(
            modifier = Modifier.padding(vertical = 12.dp)
        )

        /* 시간대, 날짜별 판매량 그래프 */
        Canvas(
            modifier = Modifier
                .fillMaxWidth()
                .height(160.dp)
        ) {
            val maxValue = buckets
                .maxOf {
                    (faceByBucket[it] ?: 0) + (cardByBucket[it] ?: 0)
                }
                .coerceAtLeast(1)

            val slot = size.width / buckets.size
            val barWidth = slot * 0.6f

            buckets.forEachIndexed { i, bucket ->
                val faceHeight =
                    size.height * (faceByBucket[bucket] ?: 0) / maxValue

                val cardHeight =
                    size.height * (cardByBucket[bucket] ?: 0) / maxValue

                val x = i * slot + (slot - barWidth) / 2

                drawRect(
                    Faceblue,
                    Offset(x, size.height - faceHeight),
                    Size(barWidth, faceHeight)
                )

                drawRect(
                    CardRed,
                    Offset(
                        x,
                        size.height - faceHeight - cardHeight
                    ),
                    Size(barWidth, cardHeight)
                )
            }
        }

        /* 그래프 날짜, 시간 표시 */
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 4.dp)
        ) {
            buckets.forEach { bucket ->
                val show = if (monthly) {
                    bucket % 5 == 1
                } else {
                    bucket % 3 == 0
                }

                Text(
                    if (show) {
                        if (monthly) "$bucket" else "${bucket}시"
                    } else {
                        ""
                    },
                    fontSize = 10.sp,
                    color = Color.Gray,
                    textAlign = TextAlign.Center,
                    softWrap = false,
                    modifier = Modifier.weight(1f)
                )
            }
        }

        /* 결제 수단별 그래프 색과 항목 이름 */
        Row(
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 8.dp)
        ) {
            Box(
                modifier = Modifier
                    .size(10.dp)
                    .background(Faceblue)
            )

            Text(
                " 얼굴 인식 결제    ",
                fontSize = 12.sp
            )

            Box(
                modifier = Modifier
                    .size(10.dp)
                    .background(CardRed)
            )

            Text(
                " 카드 결제",
                fontSize = 12.sp
            )
        }

        /* 상품별 판매 내역 */
        HorizontalDivider(
            modifier = Modifier.padding(top = 20.dp, bottom = 8.dp)
        )

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 8.dp)
        ) {
            Text(
                "상품명",
                color = Color.Gray,
                modifier = Modifier.weight(1f)
            )

            Text(
                "판매 개수",
                color = Color.Gray,
                textAlign = TextAlign.Center,
                modifier = Modifier.weight(1f)
            )

            Text(
                "판매 금액",
                color = Color.Gray,
                textAlign = TextAlign.End,
                modifier = Modifier.weight(1f)
            )
        }

        if (byProduct.isEmpty()) { // 판매 내역 없는 경우
            Text(
                errorText ?: "판매 내역이 없습니다.",
                color = Color.Gray,
                textAlign = TextAlign.Center,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 24.dp)
            )
        }

        /* 상품별 판매 수량, 금액 표시 */
        byProduct.forEach { (name, count, amount) ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 8.dp)
            ) {
                Text(
                    name,
                    modifier = Modifier.weight(1f)
                )

                Text(
                    "$count",
                    textAlign = TextAlign.Center,
                    modifier = Modifier.weight(1f)
                )

                Text(
                    "%,d 원".format(amount),
                    textAlign = TextAlign.End,
                    modifier = Modifier.weight(1f)
                )
            }
        }

        Box(
            modifier = Modifier.height(16.dp)
        )
    }

    if (showPicker) { // 달력 표시
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

@Composable
private fun SummaryItem(
    label: String,
    amount: Int,
    modifier: Modifier
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = modifier
    ) {
        Text(
            label,
            fontSize = 14.sp,
            color = Color.Gray
        )

        Text(
            "%,d 원".format(amount),
            fontSize = 16.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(top = 4.dp)
        )
    }
}