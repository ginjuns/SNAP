# Qt4 얼굴인식 키오스크

```
[KioskClient (Qt4 GUI)]  --TCP 9000-->  [KioskServer]  --QtSql-->  kiosk.db (SQLite)
```

## 빌드
```
qmake KioskSystem.pro        # 얼굴인식 시뮬레이션 모드 (OpenCV 불필요)
qmake "CONFIG+=opencv" KioskSystem.pro   # 실제 카메라 얼굴인식 (client.pro의 OpenCV 경로 수정)
make   (Windows MinGW: mingw32-make)
```
소스 파일은 UTF-8로 저장되어 있습니다. MSVC로 빌드한다면 UTF-8(BOM) 저장 또는 `/utf-8` 옵션이 필요합니다.

## 실행
1. `KioskServer` 실행 → 실행 폴더에 `kiosk.db` 생성, 포트 9000 대기
2. `KioskClient [--host 서버IP] [--fullscreen]`

## 샘플 사용자 (users 테이블)
| ID | 이름 | 권한 | 충전 잔액 |
|----|------|------|-----------|
| 1 | 관리자 | admin | 0 |
| 2 | 홍길동 | member | 50,000 |
| 3 | 김철수 | member | 30,000 |

## 얼굴 등록 (OpenCV 모드)
클라이언트 작업 폴더에 다음을 둡니다. 폴더 이름이 users.id 입니다.
```
haarcascade_frontalface_default.xml   (OpenCV 설치폴더 etc/haarcascades 에 있음)
faces/1/*.jpg    ← 관리자 얼굴 사진 여러 장
faces/2/*.jpg    ← 홍길동 얼굴 사진 여러 장
```
시뮬레이션 모드에서는 얼굴인식 창에 사용자 ID를 입력하면 인식된 것으로 처리됩니다.

## 통신 프로토콜
`common/protocol.h` 참고: `[4바이트 길이] + QDataStream(QVariantList)`
