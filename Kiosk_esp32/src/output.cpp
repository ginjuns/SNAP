#include <Arduino.h>
#include <Adafruit_NeoPixel.h>                                                // WS2812B
#include "config.h"
#include "globals.h"
#include "output.h"

static Adafruit_NeoPixel strip(numSlot, PIN_NEO, NEO_GRB + NEO_KHZ800);       // NEO_GRB: 네오픽셀 타입, NEO_KHZ800: 클럭 주파수

void initLed() {
  strip.begin();
  strip.setBrightness(80); // LED의 밝기(최대 밝기 255)
  strip.show();
  Serial.println("LED Initialized");
}

void initBuzzer() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println("Buzzer Initialized");
}

void OutputTask(void *pvParameters) {
  OutCmd m;
  for (;;) {
    xQueueReceive(outQueue, &m, portMAX_DELAY);
    switch (m.type) {
      case OutType::SHELF_LED:                                                // LED 갱신
        for (uint8_t i = 0; i < numSlot; i++) {
          bool hasItem = m.shelfMask & (1 << i);
          if (hasItem) {
            strip.setPixelColor(i, 0, 255, 0);
          } else {
            strip.setPixelColor(i, 255, 0, 0);
          }
        }
        strip.show();
        break;
      case OutType::BUZZER_SOUND:                                             // 결제 성공 알림음
        digitalWrite(PIN_BUZZER, HIGH);
        vTaskDelay(pdMS_TO_TICKS(70));
        digitalWrite(PIN_BUZZER, LOW);
        vTaskDelay(pdMS_TO_TICKS(40));
        digitalWrite(PIN_BUZZER, HIGH);
        vTaskDelay(pdMS_TO_TICKS(100));
        digitalWrite(PIN_BUZZER, LOW);
        break;
    }
  }
}
