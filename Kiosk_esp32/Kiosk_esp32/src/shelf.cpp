#include <Arduino.h>
#include <Wire.h>                                                           // I2C(VL53L0X, PCA9548A)
#include <Adafruit_VL53L0X.h>                                               // VL53L0X ToF
#include "config.h"
#include "globals.h"
#include "server.h"
#include "shelf.h"

static Adafruit_VL53L0X lox[numSlot];                                       // VL53L0X 거리 센서 4개
static SlotState slotStates[numSlot];                                       // 칸별 상태

void initI2C() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.println("I2C Bus Initialized");
}

static void selectTofChannel(uint8_t bus) {
  if (bus >= numSlot) return;

  Wire.beginTransmission(shelfCfg.pcaAddr);
  Wire.write(1 << bus);
  Wire.endTransmission();
}

void initTof() {
  for (int i = 0; i < numSlot; i++) {
    selectTofChannel(i);

    if (!lox[i].begin(0x29, false, &Wire)) {
      Serial.printf("ToF Sensor ch[%d] Initialization Failed\n", i);
    }
  }

  Serial.println("ToF Sensors Initialized");
}

static uint8_t updateShelfMask() {
  uint8_t mask = 0;

  for (uint8_t ch = 0; ch < numSlot; ch++) {
    if (slotStates[ch].hasItem) {
      mask |= (1 << ch);
    }
  }

  return mask;
}

void ShelfTask(void *pvParameters) {
  uint8_t prevMask = 0xFF; // 첫 바퀴에 무조건 LED 업데이트

  for (;;) {
    for (uint8_t ch = 0; ch < numSlot; ch++) {
      selectTofChannel(ch);

      /*ToF 센서 거리 측정*/
      VL53L0X_RangingMeasurementData_t measure;
      lox[ch].rangingTest(&measure, false);

      /*측정 데이터 검사*/
      if (measure.RangeStatus != 4) {
        slotStates[ch].lastDistanceMm = measure.RangeMilliMeter;
      } else {
        slotStates[ch].lastDistanceMm = outOfRange;
      }

      /*기준 거리 이하일 경우 "상품 있음"으로 판단*/
      bool rawHasItem = (slotStates[ch].lastDistanceMm <= shelfCfg.presentMm);

      if (rawHasItem != slotStates[ch].hasItem) {                          // 현재 확정된 상태와 다른 결과가 연속으로 나오는지 확인
        slotStates[ch].changeCnt++;
        if (slotStates[ch].changeCnt >= shelfCfg.confirmCnt) {             // 일정 횟수 이상 연속으로 다른 결과가 나오면 상태 변경
          slotStates[ch].hasItem = rawHasItem;
          slotStates[ch].changeCnt = 0;
        }
      } else {                                                             // 기존 상태와 같은 결과가 나오면 변경 카운트 초기화
        slotStates[ch].changeCnt = 0;
      }
    }

    /*진열대 비트마스크 상태 업데이트*/
    uint8_t currentMask = updateShelfMask();

    /*이전과 비교하여 진열대 상태가 변경되었는지 확인*/
    if (currentMask != prevMask) {
      OutCmd cmd;
      cmd.type = OutType::SHELF_LED;
      cmd.shelfMask = currentMask;

      xQueueSend(outQueue, &cmd, 0);

      /*바뀐 진열대만 서버로 전송*/
      for (uint8_t ch = 0; ch < numSlot; ch++) {
        bool now  = currentMask & (1 << ch);
        bool prev = prevMask & (1 << ch);
        if (now != prev) {
          String line = "stand " + String(ch + 1) + ":" + (now ? "1" : "0");
          sendToServer(line.c_str());
        }
      }

      /*전송한 상태를 이전 상태로 저장*/
      prevMask = currentMask;
    }

    /*일정한 주기로 대기*/
    vTaskDelay(pdMS_TO_TICKS(shelfCfg.scanIntervalMs));
  }
}
