#include <Arduino.h>
#include "config.h"
#include "globals.h"
#include "kiosk.h"

static KioskState currKioskState = KioskState::SLEEP;                       // 현재 키오스크 상태 (슬립 모드)
static uint32_t kioskStateStartTime = 0;                                    // 현재 키오스크 상태에 들어온 시간

static void enterState(KioskState next) {
  currKioskState = next;
  kioskStateStartTime = millis();                                           // 상태가 바뀔 때마다 시간 기록
}

static void handleEvent(KioskEvent ev) {
  switch (currKioskState) {
    case KioskState::SLEEP:
      if (ev == KioskEvent::PERSON_PRESENT) {
        enterState(KioskState::ACTIVE);
      } else if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      }
      break;
    case KioskState::ACTIVE:                                                // 활성화 -> 결제 시작 -> 결제 수단 선택/사람 이탈/취소
      if (ev == KioskEvent::PAY_START) {
        enterState(KioskState::PAYMENT_SELECT);
      } else if (ev == KioskEvent::SELECT_CARD) {                           // PAY_START 없이 바로 CARD_SELECT를 보냄...
        xTaskNotify(rfidTaskHandle, CARD_START, eSetValueWithOverwrite);
        enterState(KioskState::RFID_PAYMENT);
      } else if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      }
      break;
    case KioskState::PAYMENT_SELECT:                                        // 결제 수단 선택 -> 카드/얼굴인식/사람 이탈/취소
      if (ev == KioskEvent::SELECT_CARD) {
        xTaskNotify(rfidTaskHandle, CARD_START, eSetValueWithOverwrite);    // 카드 리더기 활성화
        enterState(KioskState::RFID_PAYMENT);
      } else if (ev == KioskEvent::SELECT_FACE) {
        enterState(KioskState::FACE_PAYMENT);
      } else if (ev == KioskEvent::PERSON_ABSENT) {
        enterState(KioskState::SLEEP);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      }
      break;
    case KioskState::RFID_PAYMENT:                                          // 카드 결제 선택 -> 성공/실패/취소
      if (ev == KioskEvent::PAYMENT_SUCCESS) {
        xTaskNotify(rfidTaskHandle, CARD_STOP, eSetValueWithOverwrite);     // 결제 성공 -> 카드 리더기 비활성화
        enterState(KioskState::PAYMENT_SUCCESS);
      } else if (ev == KioskEvent::PAYMENT_FAILED) {
        enterState(KioskState::RFID_PAYMENT);
      } else if (ev == KioskEvent::CANCEL) {
        xTaskNotify(rfidTaskHandle, CARD_STOP, eSetValueWithOverwrite);     // 결제 취소 -> 카드 리더기 비활성화
        enterState(KioskState::ACTIVE);
      }
      break;
    case KioskState::FACE_PAYMENT:                                          // 얼굴 인식 결제 선택 -> 성공/실패/취소
      if (ev == KioskEvent::PAYMENT_SUCCESS) {
        enterState(KioskState::PAYMENT_SUCCESS);
      } else if (ev == KioskEvent::PAYMENT_FAILED) {
        enterState(KioskState::FACE_PAYMENT);
      } else if (ev == KioskEvent::CANCEL) {
        enterState(KioskState::SLEEP);
      }
      break;
    case KioskState::PAYMENT_SUCCESS:                                       // 결제 성공 -> 타임아웃 -> 활성화
      if (ev == KioskEvent::TIMEOUT) {
        enterState(KioskState::ACTIVE);
      }
      break;
  }
}

void KioskTask(void *pvParameters) {
  KioskEvent ev;

  for (;;) {
    if (xQueueReceive(kioskQueue, &ev, pdMS_TO_TICKS(50)) == pdTRUE) {
      handleEvent(ev);
    }

    /*결제 성공 화면에서 일정 시간 지나면 타임아웃*/
    if (currKioskState == KioskState::PAYMENT_SUCCESS &&
      millis() - kioskStateStartTime >= SUCCESS_SCREEN_TIME) {
      handleEvent(KioskEvent::TIMEOUT);
    }
  }
}

/*KioskQueue로 이벤트 보내기*/
void sendKioskEvent(KioskEvent ev) {
  xQueueSend(kioskQueue, &ev, 0);
}
