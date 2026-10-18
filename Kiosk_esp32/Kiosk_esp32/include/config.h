#pragma once
#include "types.h"

// I2C
#define PIN_SDA
#define PIN_SCL 22
// RC522 SPI
#define PIN_RFID_SCK 18
#define PIN_RFID_MISO 19
#define PIN_RFID_MOSI 23
#define PIN_RFID_SS 5
#define PIN_RFID_RST
// WS2812B
#define PIN_NEO
// HC-SR04
#define PIN_TRIG
#define PIN_ECHO
// Buzzer
#define PIN_BUZZER 25

// 서버
#define SERVER_IP "192.168.0.6"
#define SERVER_PORT 8000

/*초음파 설정값*/
const SensorConfig sensorCfg = { 50, 70, 2, 300, 100, 30, 2000, 5, 3000 };

const float FAR_DISTANCE = 999.0;                                // 측정 무효 값
const uint32_t SUCCESS_SCREEN_TIME = 3000;                       // 결제 성공 화면 유지 시간

/*진열대 설정값*/
constexpr uint8_t numSlot = 4;                                   // 진열대 칸 개수
constexpr uint16_t outOfRange = 8190;                            // 범위 밖 ("상품 없음"으로 간주)
constexpr ShelfConfig shelfCfg = { 0x70, 150, 200, 3 };
