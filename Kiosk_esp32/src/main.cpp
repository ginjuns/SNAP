/*
* 키오스크 (PlatformIO 분할 버전)
*
* [2026-09-28]
* - 초기화 함수
* - FreeRTOS 기본 구조 설계
* - Task 생성
* - Queue 생성
* - 키오스크 상태와 이벤트 분리
*
* [2026-09-29]
* - 화면 깨우기 상태 머신 기본 구조 설계
*
* [2026-10-03]
* - echoISR(), PresenceTask() 작성
* - 초음파 센서-웨이크-키오스크 연결
* - 키오스크/웨이크 조건문 일부 수정
* - 구조체 추가하고 변수명 일부 수정
*
* [2026-10-04]
* - ShelfTask(), RFIDTask() 작성
* - OutputTask() 일부 작성
* - 카드 결제 시에만 리더기 활성화
* - MqttTask -> ServerTask 변경 (TCP 서버 통신)
*
* [2026-10-10]
* - LED 코드 추가
* - ShelfTask() 일부 수정
*
* [2026-10-14]
* - strip.show() 빠트려서 추가
* - 결제 완료 후 타임아웃, 그 다음을 SLEEP에서 ACTIVE로 바꿈
* - 사람 감지 관련해서 서버로 보내는 형식 수정
* - .ino를 .cpp로 바꾸고 분할
*
* [보류]
* - 진열대에 ToF 센서 설치 후 거리 조절
* - Queue 전송 실패할 경우...?
*
*/

#include <Arduino.h>
#include "types.h"
#include "globals.h"
#include "kiosk.h"
#include "presence.h"
#include "shelf.h"
#include "rfid.h"
#include "output.h"
#include "server.h"

static void initSerial() {
  Serial.begin(115200);
  Serial.println("System Initialization Start");
}

void setup() {
  initSerial();
  initI2C();
  initTof();
  initLed();
  initBuzzer();
  initRfid();
  initPresenceHardware();

  /*Queue 생성*/
  kioskQueue = xQueueCreate(12, sizeof(KioskEvent));
  outQueue = xQueueCreate(8, sizeof(OutCmd));
  serverQueue = xQueueCreate(12, sizeof(ServerMsg));

  initPresenceInterrupt();
  initWifi();

  /*Tasks 생성*/
  xTaskCreatePinnedToCore(KioskTask, "Kiosk", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(ShelfTask, "Shelf", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(PresenceTask, "Presence", 3072, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(RFIDTask, "RFID", 4096, NULL, 2, &rfidTaskHandle, 1);
  xTaskCreatePinnedToCore(OutputTask, "Output", 3072, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(ServerTask, "Server", 6144, NULL, 3, NULL, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
