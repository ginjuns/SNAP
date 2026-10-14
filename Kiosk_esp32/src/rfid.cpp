#include <Arduino.h>
#include <SPI.h>                                                                    // SPI(RC522)
#include <MFRC522.h>                                                                // RC522
#include "config.h"
#include "server.h"
#include "rfid.h"

static MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);                                  // RFID 리더 객체

void initRfid() {
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  mfrc522.PCD_Init();
  Serial.println("RFID Initialized");
}

void RFIDTask(void *pvParameters) {
  uint32_t cmd;
  for (;;) {
    xTaskNotifyWait(0, UINT32_MAX, &cmd, portMAX_DELAY);                            // CARD_START 대기
    if (cmd != CARD_START) continue;

    for (;;) {
      if (xTaskNotifyWait(0, UINT32_MAX, &cmd, 0) == pdTRUE && cmd == CARD_STOP) {  // CARD_STOP 확인
        break;
      }
      if (!mfrc522.PICC_IsNewCardPresent()) {                                       // 새 카드 있어?
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }
      if (!mfrc522.PICC_ReadCardSerial()) {                                         // 카드 읽었어?
        vTaskDelay(pdMS_TO_TICKS(50));
        continue;
      }

      String uid = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        uid.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
        uid.concat(String(mfrc522.uid.uidByte[i], HEX));
      }
      uid.toUpperCase();

      String line = "card:" + uid;
      // Serial.println(line);
      sendToServer(line.c_str());

      mfrc522.PICC_HaltA();                                                         // 카드 리더기 통신 종료
    }
  }
}
