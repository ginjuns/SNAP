# 얼굴인식 키오스크 (Qt5 + MySQL)

```
[KioskClient (Qt5 GUI)] ─┐
                         ├─ TCP 9000 (JSON) ─ [KioskServer] ─ MySQL (kiosk DB)
[안드로이드 앱]          ─┘
```
통신 규격은 [PROTOCOL.md](PROTOCOL.md)를 참고하세요.

## 1. MySQL 준비 (최초 1회)
```bash
sudo apt install mysql-server libqt5sql5-mysql      # Qt MySQL 드라이버 포함
sudo mysql < server/schema.sql                       # DB, 테이블, 샘플 데이터, 접속 계정 생성
cp server/server.ini.example server/server.ini       # 접속 정보 (비밀번호를 바꿨다면 수정)
```
- 테이블 구성은 `server/schema.sql`에 있습니다. 상품 사진은 `MEDIUMBLOB`(최대 16MB)에 저장합니다.
- `server.ini`에는 비밀번호가 있으므로 git에 올라가지 않습니다.

## 2. 빌드 (Qt 5.x)
```bash
sudo apt install build-essential qt5-default
qmake KioskSystem.pro && make
```

## 3. 실행
```bash
cd server && ./KioskServer        # 터미널 1: server.ini가 있는 폴더에서 실행
cd client && ./KioskClient        # 터미널 2: 서버가 다른 PC면 --host 서버IP, 전체화면은 --fullscreen
```

## 샘플 사용자
| ID | 이름 | 권한 | 충전 잔액 |
|----|------|------|-----------|
| 1 | 관리자 | admin | 0 |
| 2 | 홍길동 | member | 50,000 |
| 3 | 김철수 | member | 30,000 |

## 얼굴인식
- 기본 빌드는 **시뮬레이션 모드**입니다. 얼굴인식 창에 사용자 ID를 입력하면 인식된 것으로 처리합니다.
- 실제 카메라 인식: `sudo apt install libopencv-dev libopencv-contrib-dev` 후 `qmake "CONFIG+=opencv"`로 빌드합니다. 클라이언트 실행 폴더에 다음을 둡니다.
```
haarcascade_frontalface_default.xml   (/usr/share/opencv*/haarcascades/ 에 있음)
faces/1/*.jpg    ← users.id = 1 인 사람의 얼굴 사진 여러 장
faces/2/*.jpg
```

> 저장소의 `db.sql`은 이전 설계안입니다. 서버는 `server/schema.sql` 기준으로 동작합니다.
