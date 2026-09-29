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
* [2026-09-29]
* - 화면 깨우기 상태 머신 기본 구조 설계
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

 enum class WakeState {
  IDLE,                        // 대기 상태
  PRE_WAKE,                    // 사전 깨움
  ACTIVE,                      // 활성화 상태
  COOLDOWN                     // 재대기 시간
};

enum class WakeEvent {
  MOTION_DETECTED,             // 움직임 감지
  PRESENCE_CONFIRMED,          // 존재 확인
  PERSON_ABSENT,               // 사람 없음
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
QueueHandle_t kioskQueue;                                        // 키오스크 관련 데이터 전달 큐
QueueHandle_t outQueue;                                          // 출력 데이터 전달 큐
QueueHandle_t mqttQueue;                                         // MQTT 통신 데이터 전달 큐
TaskHandle_t  rfidTaskHandle;                                    // RFID 태스크 관리 핸들

Adafruit_VL53L0X lox[6];                                         // VL53L0X 거리 센서 6개
Adafruit_NeoPixel strip(6, PIN_NEO, NEO_GRB + NEO_KHZ800);       // NeoPixel LED 6개
MFRC522 mfrc522(PIN_RFID_SS, PIN_RFID_RST);                      // RFID 리더 객체
WiFiClient espClient;                                            // Wi-Fi 통신 객체
PubSubClient mqttClient(espClient);                              // MQTT 통신 객체

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
    case KioskState::SLEEP:  // 슬립 모드 -> 사람 감지 -> 활성화 (사람 이탈 시 슬립 모드 전환)
      if (ev == KioskEvent::PERSON_PRESENT) {
        enterState(KioskState::ACTIVE);
      } else if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      }
      break;
    case KioskState::ACTIVE:  // 활성화 -> 결제 시작 -> 결제 수단 선택 (취소 버튼 누름 또는 시간 초과 시 슬립 모드 전환)
      if (ev == KioskEvent::PAY_START) {
        enterState(KioskState::PAYMENT_SELECT);
      } else if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::PAYMENT_SELECT: // 결제 수단 선택 -> 카드/얼굴인식
      if (ev == KioskEvent::SELECT_CARD) {
        enterState(KioskState::RFID_PAYMENT);
      } else if (ev == KioskEvent::SELECT_FACE) {
        enterState(KioskState::FACE_PAYMENT);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::RFID_PAYMENT:  // 카드 결제 선택 -> 성공/실패 (실패 시 다시 결제)
      if (ev == KioskEvent::PAYMENT_SUCCESS) {
        enterState(KioskState::PAYMENT_SUCCESS);
      } else if (ev == KioskEvent::PAYMENT_FAILED) {
        enterState(KioskState::RFID_PAYMENT);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::FACE_PAYMENT:  // 얼굴 인식 결제 선택 -> 성공/실패 (실패 시 다시 결제)
      if (ev == KioskEvent::PAYMENT_SUCCESS) {
        enterState(KioskState::PAYMENT_SUCCESS);
      } else if (ev == KioskEvent::PAYMENT_FAILED) {
        enterState(KioskState::FACE_PAYMENT);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::PAYMENT_SUCCESS:  // 결제 성공 -> 타임아웃 -> 슬립 모드
      if (ev == KioskEvent::TIMEOUT) {
        enterState(KioskState::SLEEP);
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



const float ENTER_DISTANCE = 50.0;                    // 진입 거리
const float EXIT_DISTANCE  = 70.0;                    // 이탈 거리

const uint32_t PRE_WAKE_TIME = 2000;                  // 2초
const uint32_t COOLDOWN_TIME = 3000;                  // 3초

static WakeState currentState = WakeState::IDLE;


void enterState(WakeState next) {
  currentState = next;
}

void handleEvent(WakeEvent wakeEvent){
  switch (currentState) {
    case WakeState::IDLE:
      if (wakeEvent == WakeEvent::MOTION_DETECTED) {
        enterState(WakeState::PRE_WAKE);
      }
      break;
    case WakeState::PRE_WAKE:
      if (wakeEvent == WakeEvent::PRESENCE_CONFIRMED) {
        enterState(WakeState::ACTIVE);
      } else if (wakeEvent == WakeEvent::PERSON_ABSENT || wakeEvent == WakeEvent::TIMEOUT) {
        enterState(WakeState::IDLE);
      }
      break;
    case WakeState::ACTIVE:
      if (wakeEvent == WakeEvent::PERSON_ABSENT) {
        enterState(WakeState::COOLDOWN);
      }
      break;
    case WakeState::COOLDOWN:
      if (wakeEvent == WakeEvent::MOTION_DETECTED || wakeEvent == WakeEvent::PRESENCE_CONFIRMED) {
        enterState(WakeState::ACTIVE);
      } else if (wakeEvent == WakeEvent::TIMEOUT) {
        enterState(WakeState::IDLE);
      }
      break;
    }
}



void updatePresence(float distance) {
  static uint32_t stateStartTime = 0;                            // 타이머 변수(함수가 종료 후에도 이전 시간을 기억함)

  if (distance <= 0.0f) {                                        // 센서 예외 처리(너무 가까운 거리는 측정 범위 밖으로 보정)
    distance = 999.0f; 
  }

  switch (currentState) {
    case WakeState::IDLE:
      if (distance <= ENTER_DISTANCE) {                          // 일정 거리 이내로 감지되면 '움직임 감지' 발생
        stateStartTime = millis();
        handleEvent(WakeEvent::MOTION_DETECTED);
      }
      break;

    case WakeState::PRE_WAKE:
      if (distance > EXIT_DISTANCE) {                            // 검증 도중 사람이 영역 밖으로 벗어나면 '사람 없음' 발생
        handleEvent(WakeEvent::PERSON_ABSENT);
      } else if (millis() - stateStartTime >= PRE_WAKE_TIME) {   // 감지 상태가 일정 시간 이상 지속되면 '존재 확인' 발생
        handleEvent(WakeEvent::PRESENCE_CONFIRMED);
      }
      break;

    case WakeState::ACTIVE:
      if (distance > EXIT_DISTANCE) {                            // 사람이 기준 거리 밖으로 벗어나면 '사람 없음' 발생
        stateStartTime = millis();
        handleEvent(WakeEvent::PERSON_ABSENT);
      }
      break;

    case WakeState::COOLDOWN:
      if (distance <= ENTER_DISTANCE) {                          // 유예 시간 내에 사람이 다시 진입하면 '움직임 감지' 발생
        handleEvent(WakeEvent::MOTION_DETECTED); 
      } else if (millis() - stateStartTime >= COOLDOWN_TIME) {   // 유예 시간 내에 아무도 오지 않으면 '시간 초과' 발생
        handleEvent(WakeEvent::TIMEOUT);
      }
      break;
    }
}

