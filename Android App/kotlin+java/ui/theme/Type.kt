package com.example.snapapp.ui.theme

import androidx.compose.material3.Typography
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.example.snapapp.R

val NotoSansKr = FontFamily( // Noto Sans KR 폰트 설정
    Font(R.font.noto_sans_kr_regular, FontWeight.Normal),
    Font(R.font.noto_sans_kr_medium, FontWeight.Medium),
    Font(R.font.noto_sans_kr_bold, FontWeight.Bold),
)
val Typography = Typography().let { typography -> // 앱 전체 Typography에 Noto Sans KR 폰트 적용
    Typography(
        displayLarge = typography.displayLarge.copy(fontFamily = NotoSansKr),
        displayMedium = typography.displayMedium.copy(fontFamily = NotoSansKr),
        displaySmall = typography.displaySmall.copy(fontFamily = NotoSansKr),
        headlineLarge = typography.headlineLarge.copy(fontFamily = NotoSansKr),
        headlineMedium = typography.headlineMedium.copy(fontFamily = NotoSansKr),
        headlineSmall = typography.headlineSmall.copy(fontFamily = NotoSansKr),
        titleLarge = typography.titleLarge.copy(fontFamily = NotoSansKr),
        titleMedium = typography.titleMedium.copy(fontFamily = NotoSansKr),
        titleSmall = typography.titleSmall.copy(fontFamily = NotoSansKr),
        bodyLarge = typography.bodyLarge.copy(fontFamily = NotoSansKr),
        bodyMedium = typography.bodyMedium.copy(fontFamily = NotoSansKr),
        bodySmall = typography.bodySmall.copy(fontFamily = NotoSansKr),
        labelLarge = typography.labelLarge.copy(fontFamily = NotoSansKr),
        labelMedium = typography.labelMedium.copy(fontFamily = NotoSansKr),
        labelSmall = typography.labelSmall.copy(fontFamily = NotoSansKr),
    )
}




