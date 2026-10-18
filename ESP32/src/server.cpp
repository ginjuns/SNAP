#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "secrets.h"
#include "globals.h"
#include "kiosk.h"
#include "server.h"

static WiFiClient espClient;                                             // Wi-Fi 통신 객체

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Wi-Fi Initialized");
}

void ServerTask(void *pvParameters) {
  ServerMsg m;
  for (;;) {
    /*서버와 연결되어 있지 않은 경우 1초마다 연결 시도*/
    if (!espClient.connected()) {
      if (WiFi.status() == WL_CONNECTED) {
        espClient.connect(SERVER_IP, SERVER_PORT);
      }
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    /*ServerQueue에서 데이터를 가져와 서버로 보내기*/
    if (xQueueReceive(serverQueue, &m, pdMS_TO_TICKS(50)) == pdTRUE) {
      espClient.print(m.line);
      espClient.print("\n");
    }

    if (espClient.available()) {
      String line = espClient.readStringUntil('\n');

      if (line.indexOf("BUZZER") >= 0) {                                 // 결제 성공 알림음
        OutCmd cmd;
        cmd.type = OutType::BUZZER_SOUND;
        xQueueSend(outQueue, &cmd, 0);
        sendKioskEvent(KioskEvent::PAYMENT_SUCCESS);
      } else if (line.indexOf("CARD_SELECT") >= 0) {                     // 카드 결제 선택
        sendKioskEvent(KioskEvent::SELECT_CARD);
      } else if (line.indexOf("CARD_FAIL") >= 0) {                       // 결제 실패
        sendKioskEvent(KioskEvent::PAYMENT_FAILED);
      } else if (line.indexOf("CARD_CANCEL") >= 0) {                     // 취소
        sendKioskEvent(KioskEvent::CANCEL);
      }
    }
  }
}

/*서버로 보낼 메시지를 ServerQueue에 전달*/
void sendToServer(const char *line) {
  ServerMsg m;
  strncpy(m.line, line, sizeof(m.line) - 1);
  m.line[sizeof(m.line) - 1] = '\0';
  if (xQueueSend(serverQueue, &m, 0) != pdTRUE) {
    Serial.println("...Server Queue Full");
  }
}
