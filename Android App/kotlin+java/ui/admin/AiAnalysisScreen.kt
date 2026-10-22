package com.example.snapapp.ui.admin

import android.webkit.WebView
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.snapapp.ServerClient
import com.example.snapapp.ui.LoadingDialog
import com.example.snapapp.ui.theme.SnapGreen
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.time.LocalDate

/* 서버가 보낸 리포트 HTML을 화면에 맞추기 */
private fun page(body: String) = """
    <html>
    <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
            body {
                font-family: sans-serif;
                font-size: 14px;
                margin: 0;
                color: #222;
            }

            table {
                width: 100%;
                border-collapse: collapse;
            }

            td, th {
                font-size: 13px;
            }

            h3 {
                font-size: 16px;
            }
        </style>
    </head>
    <body>$body</body>
    </html>
"""

@Composable
fun AiAnalysisScreen() {
    val scope = rememberCoroutineScope()

    /* 분석 단위 선택 상태 */
    var unit by remember { mutableStateOf("day") }

    /* 로딩 상태 */
    var loading by remember { mutableStateOf(false) }

    /* 서버에서 받은 HTML 리포트 */
    var html by remember { mutableStateOf<String?>(null) }

    /* 오류 메시지 */
    var errorText by remember { mutableStateOf<String?>(null) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(horizontal = 24.dp)
    ) {
        Text(
            "AI 매출 분석",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            textAlign = TextAlign.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 16.dp, bottom = 24.dp)
        )

        /* 분석 단위 선택 */
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .border(1.dp, Color(0xFFE0E0E0), RoundedCornerShape(12.dp))
                .padding(4.dp)
        ) {
            listOf(
                "day" to "일간",
                "week" to "주간",
                "month" to "월간"
            ).forEach { (value, label) ->
                val selected = unit == value

                Box(
                    contentAlignment = Alignment.Center,
                    modifier = Modifier
                        .weight(1f)
                        .clip(RoundedCornerShape(8.dp))
                        .background(
                            if (selected) SnapGreen else Color.Transparent
                        )
                        .clickable {
                            unit = value
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

        /* AI 매출 분석 요청 */
        Button(
            onClick = {
                scope.launch {
                    loading = true

                    try {
                        val res = ServerClient.request(
                            JSONObject()
                                .put("cmd", "AI_REPORT")
                                .put("unit", unit)
                                .put("date", LocalDate.now().toString()),
                            timeoutMs = 60000
                        )

                        if (res.optBoolean("ok")) {
                            html = res.getJSONObject("data").optString("html")
                            errorText = null
                        } else {
                            errorText = res.optString(
                                "error",
                                "분석에 실패했습니다."
                            )
                        }
                    } catch (e: Exception) {
                        errorText = "서버에 연결할 수 없습니다."
                    } finally {
                        loading = false
                    }
                }
            },
            modifier = Modifier
                .fillMaxWidth()
                .padding(top = 16.dp)
                .height(48.dp)
        ) {
            Text(if (html == null) "분석 요청" else "다시 분석")
        }

        /* 오류 메시지 표시 */
        errorText?.let {
            Text(
                it,
                color = Color.Gray,
                textAlign = TextAlign.Center,
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = 24.dp)
            )
        }

        /* 서버에서 받은 HTML 리포트 표시 */
        html?.let { report ->
            AndroidView(
                factory = { context ->
                    WebView(context)
                },
                update = { webView ->
                    webView.loadDataWithBaseURL(
                        null,
                        page(report),
                        "text/html",
                        "utf-8",
                        null
                    )
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .weight(1f)
                    .padding(top = 16.dp, bottom = 16.dp)
            )
        }
    }

    if (loading) { // 로딩 창
        LoadingDialog()
    }
}