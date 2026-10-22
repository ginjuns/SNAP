package com.example.snapapp.ui.user

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationBarItemDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.unit.dp
import com.example.snapapp.R
import com.example.snapapp.ui.theme.SnapGreen

@Composable
fun UserMainScreen(
    onCharge: () -> Unit,
    onChangePassword: () -> Unit,
    onLogout: () -> Unit
) {
    /* 선택된 탭
     * 0 = 이용내역, 1 = 홈, 2 = 내 정보 */
    var selected by rememberSaveable {
        mutableIntStateOf(1)
    }

    val labels = listOf("이용내역", "홈", "내 정보")

    val icons = listOf(
        R.drawable.ic_history,
        R.drawable.ic_home,
        R.drawable.ic_mypage
    )

    Scaffold(
        bottomBar = {
            Column {
                /* 하단 내비게이션 상단 구분선 */
                HorizontalDivider(
                    thickness = 0.4.dp,
                    color = Color(0xFFEEEEEE)
                )

                NavigationBar(
                    containerColor = Color.White,
                    tonalElevation = 0.dp
                ) {
                    labels.forEachIndexed { index, label ->
                        NavigationBarItem(
                            selected = selected == index,
                            onClick = {
                                selected = index
                            },
                            icon = {
                                Icon(
                                    painter = painterResource(icons[index]),
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
        Box(
            modifier = Modifier.padding(
                bottom = innerPadding.calculateBottomPadding()
            )
        ) {
            /* 선택된 탭에 맞는 화면 표시 */
            when (selected) {
                0 -> HistoryScreen()

                1 -> HomeScreen(
                    onCharge = onCharge
                )

                2 -> MyPageScreen(
                    onChangePassword = onChangePassword,
                    onLogout = onLogout
                )
            }
        }
    }
}