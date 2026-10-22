package com.example.snapapp.ui.user

import android.content.Intent
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.example.snapapp.ui.theme.SnapAppTheme
import com.tosspayments.paymentsdk.PaymentWidget
import com.tosspayments.paymentsdk.model.AgreementStatus
import com.tosspayments.paymentsdk.model.AgreementStatusListener
import com.tosspayments.paymentsdk.model.PaymentCallback
import com.tosspayments.paymentsdk.model.TossPaymentResult
import com.tosspayments.paymentsdk.view.Agreement
import com.tosspayments.paymentsdk.view.PaymentMethod
import java.util.UUID

/*토스 결제위젯 클라이언트 키*/
private const val CLIENT_KEY = "test_gck_docs_Ovk5rk1EwkEbP0W43n07xlzm"

class TossPaymentActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        val amount = intent.getIntExtra("amount", 0)
        var agreed by mutableStateOf(false)

        /* 결제 수단과 약관을 표시하기 위한 객체 생성 */
        val paymentWidget = PaymentWidget(
            this,
            CLIENT_KEY,
            UUID.randomUUID().toString()
        )

        val methodView = PaymentMethod(this)
        val agreementView = Agreement(this)

        paymentWidget.renderPaymentMethods(methodView, amount)
        paymentWidget.renderAgreement(agreementView)
        paymentWidget.addAgreementStatusListener(
            object : AgreementStatusListener {
            override fun onAgreementStatusChanged(
                agreementStatus: AgreementStatus
            ) {
                runOnUiThread {
                    agreed = agreementStatus.agreedRequiredTerms
                }
            }
        })

        setContent {
            SnapAppTheme {
                Column(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(Color.White)
                        .systemBarsPadding()
                ) {
                    Text(
                        "결제하기",
                        fontSize = 20.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.padding(16.dp)
                    )

                    /* 결제 수단 및 약관 */
                    Column(
                        modifier = Modifier
                            .weight(1f)
                            .verticalScroll(rememberScrollState())
                    ) {
                        AndroidView(factory = { methodView },
                            modifier = Modifier.fillMaxWidth()
                        )
                        AndroidView(factory = { agreementView },
                            modifier = Modifier.fillMaxWidth()
                        )
                    }

                    /* 결제 요청 버튼 */
                    Button(
                        onClick = {
                            paymentWidget.requestPayment(
                                paymentInfo = PaymentMethod.PaymentInfo(
                                    orderId = "SNAP-" + UUID.randomUUID(),
                                    orderName = "SNAP 머니 충전"
                                ),
                                paymentCallback = object : PaymentCallback {
                                    /* 결제 성공 */
                                    override fun onPaymentSuccess(
                                        success: TossPaymentResult.Success
                                    ) {
                                        setResult( // 서버 최종 승인에 필요한 정보 전달
                                            RESULT_OK,
                                            Intent()
                                                .putExtra(
                                                    "paymentKey",
                                                    success.paymentKey
                                                )
                                                .putExtra(
                                                    "orderId",
                                                    success.orderId
                                                )
                                        )

                                        finish()
                                    }

                                    /* 결제 실패 */
                                    override fun onPaymentFailed(
                                        fail: TossPaymentResult.Fail
                                    ) {
                                        setResult(
                                            RESULT_CANCELED,
                                            Intent().putExtra(
                                                "message",
                                                fail.errorMessage
                                            )
                                        )

                                        finish()
                                    }
                                }
                            )
                        },
                        enabled = agreed,
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(16.dp)
                            .height(48.dp)
                    ) { Text("%,d원 결제하기".format(amount)) }
                }
            }
        }
    }
}