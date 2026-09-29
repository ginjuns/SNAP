/*
 * 키오스크
 * 
 * [2026-09-28]
 * - 초기화 함수
 * - FreeRTOS 기본 구조 설계
 * - Task 생성
 * - Queue 생성
 * - 키오스크 상태와 이벤트 분리
 *
 */


#include <Arduino.h>
#include <Wire.h>                // I2C(VL53L0X, PCA9548A)
#include <SPI.h>                 // SPI(RC522)
#include <Adafruit_VL53L0X.h>    // VL53L0X ToF
#include <Adafruit_NeoPixel.h>   // WS2812B
#include <MFRC522.h>             // RC522
#include <PubSubClient.h>        // MQTT통신
#include <WiFi.h>

// I2C
#define PIN_SDA
#define PIN_SCL
// RC522 SPI
#define PIN_RFID_SCK
#define PIN_RFID_MISO
#define PIN_RFID_MOSI
#define PIN_RFID_SS
#define PIN_RFID_RST
// WS2812B
#define PIN_NEO
// HC-SR04
#define PIN_TRIG
#define PIN_ECHO
// Buzzer
#define PIN_BUZZER


// 키오스크 화면 상태
enum class KioskState { 
  SLEEP,                       // 비활성화
  ACTIVE,                      // 활성화
  PAYMENT_SELECT,              // 결제 수단 선택 화면
  RFID_PAYMENT,                // 카드 결제 진행 중 (RC522 대기)
  FACE_PAYMENT,                // 얼굴 인식 결제 진행 중 (카메라 대기)
  PAYMENT_SUCCESS              // 결제 성공 안내 화면
};

// 키오스크 발생 이벤트
enum class KioskEvent { 
  PERSON_PRESENT,              // 사람 접근 감지 및 깨우기
  PERSON_ABSENT,               // 사람 이탈
  PAY_START,                   // 결제 시작
  SELECT_CARD,                 // 카드로 결제 선택
  SELECT_FACE,                 // 얼굴 인식으로 결제 선택
  PAYMENT_SUCCESS,             // 결제 성공
  PAYMENT_FAILED,              // 결제 실패
  CANCEL,                      // 취소 버튼 누름
  TIMEOUT                      // 시간 초과
 };

// Output
enum class OutType : uint8_t {
  SHELF_LED,                   // LED
  BUZZER_SOUND                 // BUZZER
};

// OutputTask에게 보내는 명령
struct OutCmd {
  OutType type;                // LED 갱신 / 결제 성공음
  uint8_t shelfMask;           // SHELF_LED일 때만 사용
};

// MQTTTask가 서버로 보낼 메시지
struct MqttMsg {
  char topic[24];              // 보낼 주소
  char payload[96];            // 보낼 내용
};

// RFIDTask 깨우기
enum : uint32_t {
  CARD_START = 1,              // 카드로 결제 on
  CARD_STOP = 2                // 가드로 결제 off
};

// Global Handles
QueueHandle_t kioskQueue;
QueueHandle_t outQueue;
QueueHandle_t mqttQueue;
TaskHandle_t  rfidTaskHandle;

Adafruit_VL53L0X lox[6];
Adafruit_NeoPixel strip(6, PIN_NEO, NEO_GRB + NEO_KHZ800);
MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup() {
  initSerial();
  initI2C();
  initTof();
  initLed();
  initBuzzer();
  initRfid();
  initPresenceHardware();

  // Queue 생성
  kioskQueue = xQueueCreate(12, sizeof(KioskEvent));
  outQueue = xQueueCreate(8, sizeof(OutCmd));
  mqttQueue = xQueueCreate(12, sizeof(MqttMsg));

  initPresenceInterrupt();

  // Network
  initWifi();
  initMqtt();
  
  // Tasks 생성
  xTaskCreatePinnedToCore(KioskTask, "Kiosk", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(ShelfTask, "Shelf", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(PresenceTask, "Presence", 3072, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(RFIDTask, "RFID", 4096, NULL, 2, &rfidTaskHandle, 1);
  xTaskCreatePinnedToCore(OutputTask, "Output", 3072, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(MQTTTask, "MQTT", 6144, NULL, 3, NULL, 0);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}


void initSerial() {
  Serial.begin(115200);
  Serial.println("System Initialization Start");
}

void initI2C() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Serial.println("I2C Bus Initialized");
}

void initTof() {
  for (int i = 0; i < 6; i++) {
    selectTofChannel(i);

    if (!lox[i].begin(0x29, false, &Wire)) {
      Serial.printf("ToF Sensor ch[%d] Initialization Failed\n", i);
    }
  }

  Serial.println("ToF Sensors Initialized");
}

void initLed() {
  strip.begin();
  strip.show();
  Serial.println("LED Initialized");
}

void initBuzzer() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  Serial.println("Buzzer Initialized");
}

void initRfid() {
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  mfrc522.PCD_Init();
  Serial.println("RFID Initialized");
}

void initPresenceHardware() {
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  Serial.println("Presence Sensor Initialized");
}

void initPresenceInterrupt() {
  attachInterrupt(digitalPinToInterrupt(PIN_ECHO), echoISR, CHANGE);
  Serial.println("Presence Interrupt Initialized");
}

void initWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("Wi-Fi Initialized");
}

void initMqtt() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  Serial.println("MQTT Initialized");
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


static KioskState currentState = KioskState::SLEEP;

void enterState(KioskState next) {

  currentState = next;
}

void handleEvent(KioskEvent ev) {
  switch (currentState) {
    case KioskState::SLEEP:
      if (ev == KioskEvent::PERSON_PRESENT) {
        enterState(KioskState::ACTIVE);
      }
      break;
    case KioskState::ACTIVE:
      if (ev == KioskEvent::PAY_START) {
        enterState(KioskState::PAYMENT_SELECT);
      }
      break;
    case KioskState::PAYMENT_SELECT:
      if (ev == KioskEvent::SELECT_CARD) {
        enterState(KioskState::RFID_PAYMENT);
      }
      else if (ev == KioskEvent::SELECT_FACE) {
        enterState(KioskState::FACE_PAYMENT);
      }
      break;

    default:
    break;
  }
}


void KioskTask(void *pvParameters) {
  KioskEvent ev;

  for (;;) {
    if (xQueueReceive(kioskQueue, &ev, pdMS_TO_TICKS(50)) == pdTRUE) {
      handleEvent(ev);
    }
  }
}

void ShelfTask(void *pvParameters) {
  for (;;) {
    
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void PresenceTask(void *pvParameters) {
  for (;;) {

    vTaskDelay(pdMS_TO_TICKS(150));
  }
}

void RFIDTask(void *pvParameters) {
  uint32_t cmd;
  for (;;) {
    xTaskNotifyWait(0, UINT32_MAX, &cmd, portMAX_DELAY);
    if (cmd != CARD_START) continue;
    
  }
}

void OutputTask(void *pvParameters) {
  OutCmd m;
  for (;;) {
    xQueueReceive(outQueue, &m, portMAX_DELAY);
    switch (m.type) {
      case OutType::SHELF_LED:      // LED 갱신
      break;
      case OutType::BUZZER_SOUND:   // 결제 성공 알림음
      break;
    }
  }
}

void MQTTTask(void *pvParameters) {
  MqttMsg m;
  for (;;) {
    mqttClient.loop();
    if (xQueueReceive(mqttQueue, &m, pdMS_TO_TICKS(50)) == pdTRUE) {
      mqttClient.publish(m.topic, m.payload);
    }
  }
}

