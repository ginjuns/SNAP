#pragma once
#include <Arduino.h>

/*Global Handles*/
extern QueueHandle_t kioskQueue;                                 // 키오스크 관련
extern QueueHandle_t outQueue;                                   // 출력 데이터 전달 큐
extern QueueHandle_t serverQueue;                                // 통신 데이터 전달 큐
extern TaskHandle_t  rfidTaskHandle;                             // RFID 태스크 관리 핸들
