#include "globals.h"

QueueHandle_t kioskQueue;                                               // 키오스크 관련
QueueHandle_t outQueue;                                                 // 출력 데이터 전달 큐
QueueHandle_t serverQueue;                                              // 통신 데이터 전달 큐
TaskHandle_t  rfidTaskHandle;                                           // RFID 태스크 관리 핸들
