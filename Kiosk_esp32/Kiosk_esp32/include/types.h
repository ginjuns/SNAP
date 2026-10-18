#pragma once
#include <Arduino.h>

/*키오스크 화면 상태*/
enum class KioskState : uint8_t {
  SLEEP,                                                         // 비활성화
  ACTIVE,                                                        // 활성화
  PAYMENT_SELECT,                                                // 결제 수단 선택 화면
  RFID_PAYMENT,                                                  // 카드 결제 진행 중 (RC522 대기)
  FACE_PAYMENT,                                                  // 얼굴 인식 결제 진행 중
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

/*Output*/
enum class OutType : uint8_t {
  SHELF_LED,                                                     // LED
  BUZZER_SOUND                                                   // BUZZER
};

/*OutputTask에게 보내는 명령*/
struct OutCmd {
  OutType type;                                                  // LED 갱신 / 결제 성공음
  uint8_t shelfMask;                                             // SHELF_LED일 때만 사용
};

/*ServerTask가 서버로 보낼 메시지*/
struct ServerMsg {
  char line[96];                                                 // 서버로 보낼 메시지 한 줄
};

/*RFIDTask 깨우기*/
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

/*깨우기 런타임 상태*/
struct WakeRuntime {
  WakeState state;                                               // 현재의 WakeState
  uint8_t leavingCnt;                                            // 이탈 카운트
  uint32_t enteredTimeMs;                                        // 현재 상태에 진입한 시간
};

/*초음파 공유 데이터 (ISR ↔ PresenceTask)*/
struct SensorSharedResources {
  uint32_t startTimeUs;                                          // 시작 시간
  uint32_t pulseDurationUs;                                      // 지속 시간
  bool echoReady;                                                // ECHO를 받을 준비가 되어 있는지
  bool doneFlag;                                                 // 완료 플래그
};

/*진열대 공통 설정*/
struct ShelfConfig {
  uint8_t pcaAddr;                                               // PCA9548A 주소
  uint16_t presentMm;                                            // "상품 있음"의 기준 거리
  uint32_t scanIntervalMs;                                       // 'numSlot'칸 한 바퀴 측정 주기 (ms)
  uint8_t confirmCnt;                                            // 상품 유무가 바뀌려면 필요한 연속 확인 횟수
};

/*슬롯 상태*/
struct SlotState {
  uint16_t lastDistanceMm;                                       // 마지막 측정 거리
  bool hasItem;                                                  // 현재 확정된 상품 유무
  uint8_t changeCnt;                                             // 현재 판단과 반대 결과가 나온 횟수
};
