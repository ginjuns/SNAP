package com.example.snapapp.ui.admin

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import com.example.snapapp.R
import com.example.snapapp.ui.theme.SnapGreen

/* 관리자 메인 화면 */
@Composable
fun AdminMainScreen(onLogout: () -> Unit) {

    /* 현재 선택된 하단 메뉴 상태 */
    var selected by rememberSaveable { mutableIntStateOf(1) } // 매출 확인 페이지가 기본

    /* 로그아웃 확인 창 상태 */
    var showLogout by remember { mutableStateOf(false) }

    /* 하단 메뉴 이름과 아이콘 */
    val labels = listOf("상품 관리", "판매량", "AI 매출 분석", "로그아웃")
    val icons = listOf(
        R.drawable.ic_inven,
        R.drawable.ic_chart,
        R.drawable.ic_ai,
        R.drawable.ic_logout
    )

    /* 화면 기본 구조 및 하단 네비게이션바 */
    Scaffold(
        bottomBar = {
            Column {
                HorizontalDivider(
                    thickness = 0.4.dp,
                    color = Color.LightGray
                )

                NavigationBar(
                    containerColor = Color.White,
                    tonalElevation = 0.dp
                ) {
                    /* 각 항목을 하단 메뉴에 표시 */
                    labels.forEachIndexed { index, label ->
                        NavigationBarItem(
                            selected = selected == index,

                            /* 로그아웃 선택 시 확인 창 표시 -> 선택한 메뉴에 맞게 화면 변경 */
                            onClick = {
                                if (index == 3) {
                                    showLogout = true
                                } else {
                                    selected = index
                                }
                            },

                            icon = {
                                Icon(
                                    painterResource(icons[index]),
                                    contentDescription = label
                                )
                            },

                            label = {
                                Text(label)
                            },

                            colors = NavigationBarItemDefaults.colors(
                                selectedIconColor = SnapGreen,
                                selectedTextColor = SnapGreen,
                                indicatorColor = Color.Transparent
                            )
                        )
                    }
                }
            }
        }
    ) { innerPadding ->

        /* 선택된 메뉴에 해당하는 화면 표시 */
        Box(
            modifier = Modifier.padding(innerPadding)
        ) {
            when (selected) {
                0 -> ProductScreen()     // 상품 관리 화면
                1 -> SalesScreen()       // 판매량 화면
                2 -> AiAnalysisScreen()  // AI 매출 분석 화면
            }
        }
    }

    /* 로그아웃 확인 창 */
    if (showLogout) {
        AlertDialog(
            onDismissRequest = {
                showLogout = false
            },    // 바깥 영역을 누르면 확인 창 닫기

            text = {
                Text("로그아웃 하시겠습니까?")
            },

            confirmButton = {
                TextButton(
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