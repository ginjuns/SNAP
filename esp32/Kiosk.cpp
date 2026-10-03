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
* [2026-10-03]
* - echoISR(), PresenceTask() 작성
* - 초음파 센서-웨이크-키오스크 연결
* - 키오스크/웨이크 조건문 일부 수정
* - 구조체 추가하고 변수명 일부 수정
*
*/


#include <Arduino.h>
#include <Wire.h>                                                // I2C(VL53L0X, PCA9548A)
#include <SPI.h>                                                 // SPI(RC522)
#include <Adafruit_VL53L0X.h>                                    // VL53L0X ToF
#include <Adafruit_NeoPixel.h>                                   // WS2812B
#include <MFRC522.h>                                             // RC522
#include <PubSubClient.h>                                        // MQTT통신
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


/*키오스크 화면 상태*/
enum class KioskState : uint8_t { 
  SLEEP,                                                         // 비활성화
  ACTIVE,                                                        // 활성화
  PAYMENT_SELECT,                                                // 결제 수단 선택 화면
  RFID_PAYMENT,                                                  // 카드 결제 진행 중 (RC522 대기)
  FACE_PAYMENT,                                                  // 얼굴 인식 결제 진행 중 (카메라 대기)
  PAYMENT_SUCCESS                                                // 결제 성공 안내 화면
};

/*키오스크 발생 이벤트*/
enum class KioskEvent : uint8_t { 
  PERSON_PRESENT,                                                // 사람 접근 감지 및 깨우기
  PERSON_ABSENT,                                                 // 사람 이탈
  PAY_START,                                                     // 결제 시작
  SELECT_CARD,                                                   // 카드로 결제 선택
  SELECT_FACE,                                                   // 얼굴 인식으로 결제 선택
  PAYMENT_SUCCESS,                                               // 결제 성공
  PAYMENT_FAILED,                                                // 결제 실패
  CANCEL,                                                        // 취소 버튼 누름
  TIMEOUT                                                        // 시간 초과
 };

/*화면 깨우기 상태*/
 enum class WakeState : uint8_t {
  IDLE,                                                          // 대기 상태
  PRE_WAKE,                                                      // 사전 깨움
  ACTIVE,                                                        // 활성화 상태
  COOLDOWN                                                       // 재대기 시간
};

/*화면 깨우기 이벤트*/
enum class WakeEvent : uint8_t {
  MOTION_DETECTED,                                               // 움직임 감지
  PRESENCE_CONFIRMED,                                            // 존재 확인
  PERSON_ABSENT,                                                 // 사람 없음
  TIMEOUT                                                        // 시간 초과
};

// Output
enum class OutType : uint8_t {
  SHELF_LED,                                                     // LED
  BUZZER_SOUND                                                   // BUZZER
};

// OutputTask에게 보내는 명령
struct OutCmd {
  OutType type;                                                  // LED 갱신 / 결제 성공음
  uint8_t shelfMask;                                             // SHELF_LED일 때만 사용
};

// MQTTTask가 서버로 보낼 메시지
struct MqttMsg {
  char topic[24];                                                // 보낼 주소
  char payload[96];                                              // 보낼 내용
};

// RFIDTask 깨우기
enum CardCmd : uint8_t {
  CARD_START = 1,                                                // 카드로 결제 on
  CARD_STOP = 2                                                  // 카드로 결제 off
};

/*초음파 설정값*/
struct SensorConfig {
  uint16_t detectCm;                                             // 감지 기준 거리
  uint16_t releaseCm;                                            // 해제 기준 거리
  uint16_t minDistanceCm;                                        // 최소 거리
  uint16_t maxDistanceCm;                                        // 최대 거리
  uint32_t intervalMs;                                           // 측정 주기
  uint32_t echoTimeoutMs;                                        // ECHO 타임아웃
  uint32_t preWakeMs;                                            // PRE_WAKE 유지 시간
  uint8_t leaveCount;                                            // 이탈 확정 횟수
  uint32_t cooldownDurationMs;                                   // COOLDOWN 유지 기간
};

const SensorConfig sensorCfg = { 50, 70, 2, 300, 100, 30, 2000, 5, 3000 };

const float FAR_DISTANCE = 999.0;                                // 측정 무효 값
const uint32_t SUCCESS_SCREEN_TIME = 3000;                       // 결제 성공 화면 유지 시간

/*깨우기 런타임 상태*/
struct WakeRuntime {
  WakeState state;                                               // 현재의 WakeState
  uint8_t detectingCnt;                                          // 접근 카운트
  uint8_t leavingCnt;                                            // 이탈 카운트
  float lastDistanceCm;                                          // 마지막 측정 거리
  uint32_t enteredTimeMs;                                        // 현재 상태에 진입한 시간
};

static WakeRuntime wake = { WakeState::IDLE, 0, 0, FAR_DISTANCE, 0 };

/*공유 데이터*/
static volatile struct SensorSharedResources {
  uint32_t startTimeUs;                                          // 시작 시간
  uint32_t pulseDurationUs;                                      // 지속 시간
  bool echoReady;                                                // ECHO를 받을 준비가 되어 있는지
  bool doneFlag;                                                 // 완료 플래그
} sharedRes;

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

/*초음파 센서 인터럽트*/
void IRAM_ATTR echoISR() {
  uint32_t now = micros();
  if (sharedRes.echoReady != false) {
    if (digitalRead(PIN_ECHO) == HIGH) {                         // ECHO: LOW --> HIGH
      sharedRes.startTimeUs = now;
    } else {                                                     // ECHO: HIGH --> LOW
      sharedRes.pulseDurationUs = now - sharedRes.startTimeUs;   // 반사되어 돌아온 시간
      sharedRes.doneFlag = true;                                 // 측정 완료 신호
      sharedRes.echoReady = false;                               // ECHO 초기화
    }
  }
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


static KioskState currKioskState = KioskState::SLEEP;   // 현재 키오스크 상태 (슬립 모드)
static uint32_t kioskStateStartTime = 0;                // 현재 키오스크 상태에 들어온 시간

void enterState(KioskState next) {
  currKioskState = next;
  kioskStateStartTime = millis();                       // 상태가 바뀔 때마다 시간 기록
}

void handleEvent(KioskEvent ev) {
  switch (currKioskState) {
    case KioskState::SLEEP:
      if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      }
      break;
    case KioskState::ACTIVE:                            // 활성화 -> 결제 시작 -> 결제 수단 선택/사람 이탈/취소
      if (ev == KioskEvent::PAY_START) {
        enterState(KioskState::PAYMENT_SELECT);
      } else if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::PAYMENT_SELECT:                    // 결제 수단 선택 -> 카드/얼굴인식/사람 이탈/취소
      if (ev == KioskEvent::SELECT_CARD) {
        enterState(KioskState::RFID_PAYMENT);
      } else if (ev == KioskEvent::SELECT_FACE) {
        enterState(KioskState::FACE_PAYMENT);
      } else if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::RFID_PAYMENT:                      // 카드 결제 선택 -> 성공/실패/취소 (사람 이탈 넣을지 말지 고민중)
      if (ev == KioskEvent::PAYMENT_SUCCESS) {
        enterState(KioskState::PAYMENT_SUCCESS);
      } else if (ev == KioskEvent::PAYMENT_FAILED) {
        enterState(KioskState::RFID_PAYMENT);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::FACE_PAYMENT:                      // 얼굴 인식 결제 선택 -> 성공/실패/취소 (사람 이탈 넣을지 말지 고민중)
      if (ev == KioskEvent::PAYMENT_SUCCESS) {
        enterState(KioskState::PAYMENT_SUCCESS);
      } else if (ev == KioskEvent::PAYMENT_FAILED) {
        enterState(KioskState::FACE_PAYMENT);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      } 
      break;
    case KioskState::PAYMENT_SUCCESS:                   // 결제 성공 -> 타임아웃 -> 슬립 모드
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

    // 결제 성공 화면에서 일정 시간 지나면 타임아웃
    if (currKioskState == KioskState::PAYMENT_SUCCESS &&
      millis() - kioskStateStartTime >= SUCCESS_SCREEN_TIME) {
      handleEvent(KioskEvent::TIMEOUT);
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
    // 측정 준비
    sharedRes.doneFlag = false;
    sharedRes.echoReady = true;

    // 측정
    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    // 측정 완료
    uint32_t waitStart = millis();
    while (!sharedRes.doneFlag && millis() - waitStart < sensorCfg.echoTimeoutMs) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    // 거리 계산
    float distance;
    if (sharedRes.doneFlag) {
      distance = sharedRes.pulseDurationUs / 58.0;
    } else {
      sharedRes.echoReady = false;
      distance = FAR_DISTANCE;
    }

    // 웨이크 상태 머신에 거리 전달
    updatePresence(distance);

    // 다음 측정까지 대기
    vTaskDelay(pdMS_TO_TICKS(sensorCfg.intervalMs));
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

/*KioskQueue로 이벤트 보내기*/
void sendKioskEvent(KioskEvent ev) {
  xQueueSend(kioskQueue, &ev, 0);
}

void enterState(WakeState next) {
  WakeState prev = wake.state;                                                                    // 이전 상태 기억
  wake.state = next;
  wake.enteredTimeMs = millis();                                                                  // 상태가 바뀔 때마다 시간 기록

  if (prev == WakeState::PRE_WAKE && next == WakeState::ACTIVE) {                                 // PRE_WAKE에서 ACTIVE로 전환되면 사람 존재 확정
    sendKioskEvent(KioskEvent::PERSON_PRESENT);
  } else if (prev == WakeState::COOLDOWN && next == WakeState::IDLE) {                            // COOLDOWN에서 IDLE로 전환되면 사람 이탈 확정     
    sendKioskEvent(KioskEvent::PERSON_ABSENT);
  }
}

void handleEvent(WakeEvent wakeEvent){
  switch (wake.state) {
    case WakeState::IDLE:
      if (wakeEvent == WakeEvent::MOTION_DETECTED) {                                               // 움직임 감지되면 PRE_WAKE
        enterState(WakeState::PRE_WAKE);
      }
      break;
    case WakeState::PRE_WAKE:
      if (wakeEvent == WakeEvent::PRESENCE_CONFIRMED) {                                            // 사람 존재 확인되면 ACTIVE
        enterState(WakeState::ACTIVE);
      } else if (wakeEvent == WakeEvent::PERSON_ABSENT || wakeEvent == WakeEvent::TIMEOUT) {       // 이탈 또는 시간 초과 시 다시 IDLE
        enterState(WakeState::IDLE);
      }
      break;
    case WakeState::ACTIVE:
      if (wakeEvent == WakeEvent::PERSON_ABSENT) {                                                 // 사람 이탈 확인되면 COOLDOWN
        enterState(WakeState::COOLDOWN);
      }
      break;
    case WakeState::COOLDOWN:
      if (wakeEvent == WakeEvent::MOTION_DETECTED || wakeEvent == WakeEvent::PRESENCE_CONFIRMED) { // 다시 접근하면 ACTIVE
        enterState(WakeState::ACTIVE);
      } else if (wakeEvent == WakeEvent::TIMEOUT) {                                                // 시간 초과되면 IDLE
        enterState(WakeState::IDLE);
      }
      break;
    }
}

void updatePresence(float distance) {
  if (distance < sensorCfg.minDistanceCm || distance > sensorCfg.maxDistanceCm) {     // 측정 범위를 초과하면 무효
    distance = FAR_DISTANCE;
  }

  switch (wake.state) {
    case WakeState::IDLE:
      if (distance <= sensorCfg.detectCm) {                                           // 일정 거리 이내로 감지되면 '움직임 감지' 발생
        handleEvent(WakeEvent::MOTION_DETECTED);
      }
      break;

    case WakeState::PRE_WAKE:
      if (distance > sensorCfg.releaseCm) {                                           // 검증 도중 사람이 영역 밖으로 벗어나면 '사람 없음' 발생
        handleEvent(WakeEvent::PERSON_ABSENT);
      } else if (millis() - wake.enteredTimeMs >= sensorCfg.preWakeMs) {                  // 감지 상태가 일정 시간 이상 지속되면 '존재 확인' 발생
        handleEvent(WakeEvent::PRESENCE_CONFIRMED);
      }
      break;

    case WakeState::ACTIVE:
      if (distance > sensorCfg.releaseCm) {                                           // 기준 거리보다 멀어지면 이탈
        wake.leavingCnt++;
        if (wake.leavingCnt >= sensorCfg.leaveCount) {                                // 일정 횟수 이상 연속적 이탈 확인되면 '사람 없음' 발생
          wake.leavingCnt = 0;
          handleEvent(WakeEvent::PERSON_ABSENT);
        }
      } else {                                                                        // 다시 범위 안으로 들어오면 이탈 카운트 초기화
        wake.leavingCnt = 0;
      }
      break;

    case WakeState::COOLDOWN:
      if (distance <= sensorCfg.detectCm) {                                           // 유예 시간 내에 사람이 다시 진입하면 '움직임 감지' 발생
        handleEvent(WakeEvent::MOTION_DETECTED); 
      } else if (millis() - wake.enteredTimeMs >= sensorCfg.cooldownDurationMs) {     // 유예 시간 내에 아무도 오지 않으면 '시간 초과' 발생
        handleEvent(WakeEvent::TIMEOUT);
      }
      break;
    }
}
