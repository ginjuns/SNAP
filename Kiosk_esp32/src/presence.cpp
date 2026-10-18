#include <Arduino.h>
#include "config.h"
#include "kiosk.h"
#include "server.h"
#include "presence.h"

static WakeRuntime wake = { WakeState::IDLE, 0, 0 };                                // 깨우기 런타임 상태
static volatile SensorSharedResources sharedRes;                                    // 공유 데이터

/*초음파 센서 인터럽트*/
static void IRAM_ATTR echoISR() {
  uint32_t now = micros();
  if (sharedRes.echoReady != false) {
    if (digitalRead(PIN_ECHO) == HIGH) {                                            // ECHO: LOW --> HIGH
      sharedRes.startTimeUs = now;
    } else {                                                                        // ECHO: HIGH --> LOW
      sharedRes.pulseDurationUs = now - sharedRes.startTimeUs;                      // 반사되어 돌아온 시간
      sharedRes.doneFlag = true;                                                    // 측정 완료 신호
      sharedRes.echoReady = false;                                                  // ECHO 초기화
    }
  }
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

static void enterState(WakeState next) {
  wake.state = next;
  wake.enteredTimeMs = millis();                                                    // 상태가 바뀔 때마다 시간 기록
}

static void handleEvent(WakeEvent wakeEvent) {
  switch (wake.state) {
    case WakeState::IDLE:
      if (wakeEvent == WakeEvent::MOTION_DETECTED) {                                // 움직임 감지되면 PRE_WAKE
        enterState(WakeState::PRE_WAKE);
      }
      break;
    case WakeState::PRE_WAKE:
      if (wakeEvent == WakeEvent::PRESENCE_CONFIRMED) {                             // 사람 존재 확인되면 ACTIVE
        sendKioskEvent(KioskEvent::PERSON_PRESENT);
        sendToServer("person:1");
        enterState(WakeState::ACTIVE);
      } else if (wakeEvent == WakeEvent::PERSON_ABSENT) {                           // 이탈 시 다시 IDLE
        enterState(WakeState::IDLE);
      }
      break;
    case WakeState::ACTIVE:
      if (wakeEvent == WakeEvent::PERSON_ABSENT) {                                  // 사람 이탈 확인되면 COOLDOWN
        enterState(WakeState::COOLDOWN);
      }
      break;
    case WakeState::COOLDOWN:
      if (wakeEvent == WakeEvent::MOTION_DETECTED) {                                // 다시 접근하면 ACTIVE
        enterState(WakeState::ACTIVE);
      } else if (wakeEvent == WakeEvent::TIMEOUT) {                                 // 시간 초과되면 IDLE
        sendKioskEvent(KioskEvent::PERSON_ABSENT);
        sendToServer("person:0");
        enterState(WakeState::IDLE);
      }
      break;
  }
}

static void updatePresence(float distance) {
  if (distance < sensorCfg.minDistanceCm || distance > sensorCfg.maxDistanceCm) {   // 측정 범위를 초과하면 무효
    distance = FAR_DISTANCE;
  }

  switch (wake.state) {
    case WakeState::IDLE:
      if (distance <= sensorCfg.detectCm) {                                         // 일정 거리 이내로 감지되면 '움직임 감지' 발생
        handleEvent(WakeEvent::MOTION_DETECTED);
      }
      break;

    case WakeState::PRE_WAKE:
      if (distance > sensorCfg.releaseCm) {                                         // 검증 도중 사람이 영역 밖으로 벗어나면 '사람 없음' 발생
        handleEvent(WakeEvent::PERSON_ABSENT);
      } else if (millis() - wake.enteredTimeMs >= sensorCfg.preWakeMs) {            // 감지 상태가 일정 시간 이상 지속되면 '존재 확인' 발생
        handleEvent(WakeEvent::PRESENCE_CONFIRMED);
      }
      break;

    case WakeState::ACTIVE:
      if (distance > sensorCfg.releaseCm) {                                         // 기준 거리보다 멀어지면 이탈
        wake.leavingCnt++;
        if (wake.leavingCnt >= sensorCfg.leaveCount) {                              // 일정 횟수 이상 연속적 이탈 확인되면 '사람 없음' 발생
          wake.leavingCnt = 0;
          handleEvent(WakeEvent::PERSON_ABSENT);
        }
      } else {                                                                      // 다시 범위 안으로 들어오면 이탈 카운트 초기화
        wake.leavingCnt = 0;
      }
      break;

    case WakeState::COOLDOWN:
      if (distance <= sensorCfg.detectCm) {                                         // 유예 시간 내에 사람이 다시 진입하면 '움직임 감지' 발생
        handleEvent(WakeEvent::MOTION_DETECTED);
      } else if (millis() - wake.enteredTimeMs >= sensorCfg.cooldownDurationMs) {   // 유예 시간 내에 아무도 오지 않으면 '시간 초과' 발생
        handleEvent(WakeEvent::TIMEOUT);
      }
      break;
  }
}

void PresenceTask(void *pvParameters) {
  for (;;) {
    /*측정 준비*/
    sharedRes.doneFlag = false;
    sharedRes.echoReady = true;

    digitalWrite(PIN_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);

    /*측정 완료*/
    uint32_t waitStart = millis();
    while (!sharedRes.doneFlag && millis() - waitStart < sensorCfg.echoTimeoutMs) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }

    /*거리 계산*/
    float distance;
    if (sharedRes.doneFlag) {
      distance = sharedRes.pulseDurationUs / 58.0;
    } else {
      sharedRes.echoReady = false;
      distance = FAR_DISTANCE;
    }

    /*웨이크 상태 머신에 거리 전달*/
    updatePresence(distance);

    /*다음 측정까지 대기*/
    vTaskDelay(pdMS_TO_TICKS(sensorCfg.intervalMs));
  }
}
