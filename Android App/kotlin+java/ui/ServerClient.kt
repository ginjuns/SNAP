package com.example.snapapp

import android.graphics.Bitmap
import android.util.Base64
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.net.InetSocketAddress
import java.net.Socket

object ServerClient {

    private const val HOST = "192.168.0.6"
    private const val PORT = 9000

    /* 로그인 시 서버에서 받은 토큰 (관리자 기능에서 사용) */
    var token = ""

    /* JSON 요청을 전송하고 서버 응답을 한 줄로 받아오기
    * timeoutMs: 서버 응답을 기다리는 최대 시간 */
    suspend fun request(
        body: JSONObject,
        timeoutMs: Int = 10000
    ): JSONObject = withContext(Dispatchers.IO) {

        /* 토큰이 있고 요청 본문에 토큰이 없으면 자동으로 추가 */
        if (token.isNotEmpty() && !body.has("token")) {
            body.put("token", token)
        }

        Socket().use { socket ->

            /* 서버 연결 (최대 5초 대기) */
            socket.connect(
                InetSocketAddress(HOST, PORT),
                5000
            )

            /* 서버 응답 대기 시간 설정 */
            socket.soTimeout = timeoutMs

            /* JSON 요청 전송 */
            val writer = socket.getOutputStream()
                .bufferedWriter(Charsets.UTF_8)

            writer.write(body.toString() + "\n")
            writer.flush()

            /* 서버 응답 수신 */
            val line = socket.getInputStream()
                .bufferedReader(Charsets.UTF_8)
                .readLine()
                ?: throw IOException("서버 응답 없음")

            JSONObject(line)
        }
    }
}

/* Bitmap을 640px 이하의 JPEG로 압축한 뒤 Base64 문자열로 변환 */
fun Bitmap.toBase64(): String {

    /* 이미지의 긴 변이 640px을 넘으면 비율을 유지하며 축소 */
    val scale = 640f / maxOf(width, height)

    val small = if (scale < 1f) {
        Bitmap.createScaledBitmap(
            this,
            (width * scale).toInt(),
            (height * scale).toInt(),
            true
        )
    } else {
        this
    }

    /* JPEG로 압축 */
    val out = ByteArrayOutputStream()
    small.compress(Bitmap.CompressFormat.JPEG, 80, out)

    /* 줄바꿈 없는 Base64 문자열 반환 */
    return Base64.encodeToString(
        out.toByteArray(),
        Base64.NO_WRAP
    )
}