package com.example.snapapp.ui

import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.example.snapapp.ServerClient
import com.example.snapapp.ui.admin.AdminMainScreen
import com.example.snapapp.ui.auth.LoginScreen
import com.example.snapapp.ui.auth.SignUpScreen
import com.example.snapapp.ui.auth.SplashScreen
import com.example.snapapp.ui.user.ChargeScreen
import com.example.snapapp.ui.user.PasswordChangeScreen
import com.example.snapapp.ui.user.UserMainScreen

@Composable
fun AppNavigation() {

    val navController = rememberNavController()
    val context = LocalContext.current

    val prefs = context.getSharedPreferences(
        "loginPrefs",
        Context.MODE_PRIVATE
    )

    /* 로그아웃 처리 */
    val logout: () -> Unit = {
        prefs.edit().clear().apply()
        ServerClient.token = ""

        navController.navigate("login") {
            popUpTo(navController.graph.id) {
                inclusive = true
            }
        }
    }

    NavHost(
        navController = navController,
        startDestination = "splash"
    ) {

        /* 스플래시 화면 */
        composable("splash") {
            SplashScreen(
                onFinish = {
                    // 저장된 로그인 토큰 복원
                    ServerClient.token =
                        prefs.getString("token", "") ?: ""

                    // 자동 로그인 및 사용자 권한 확인
                    val next = when {
                        !prefs.getBoolean("autoLogin", false) -> "login"
                        prefs.getString("role", "") == "ADMIN" -> "admin"
                        else -> "user"
                    }

                    navController.navigate(next) {
                        popUpTo("splash") {
                            inclusive = true
                        }
                    }
                }
            )
        }

        /* 로그인 화면 */
        composable("login") {
            LoginScreen(
                onLoginSuccess = { role ->
                    navController.navigate(
                        if (role == "ADMIN") "admin" else "user"
                    ) {
                        popUpTo("login") {
                            inclusive = true
                        }
                    }
                },
                onSignUp = {
                    navController.navigate("signup")
                }
            )
        }

        /* 회원가입 화면 */
        composable("signup") {
            SignUpScreen(
                onDone = {
                    navController.popBackStack()
                }
            )
        }

        /* 사용자 메인 화면 */
        composable("user") {
            UserMainScreen(
                onCharge = {
                    navController.navigate("charge")
                },
                onChangePassword = {
                    navController.navigate("password")
                },
                onLogout = logout
            )
        }

        /* 충전 화면 */
        composable("charge") {
            ChargeScreen(
                onDone = {
                    navController.popBackStack()
                }
            )
        }

        /* 비밀번호 변경 화면 */
        composable("password") {
            PasswordChangeScreen(
                onDone = {
                    navController.popBackStack()
                }
            )
        }

        /* 관리자 메인 화면 */
        composable("admin") {
            AdminMainScreen(
                onLogout = logout
            )
        }
    }
}