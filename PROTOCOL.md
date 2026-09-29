# 서버 통신 규격 (키오스크 / 안드로이드 앱 공용)

## 연결
- **TCP**, 서버 IP의 **9000번 포트**
- 요청과 응답 모두 **UTF-8 JSON 객체 한 줄 + 줄바꿈(`\n`)**
- 연결 하나로 여러 요청을 차례로 보내도 되고, 요청마다 새로 연결해도 됩니다.
- 한 줄 최대 16MB. 숫자 필드(`userId`, `price` 등)는 **문자열이 아닌 숫자**로 보냅니다.

```
요청: {"cmd":"LOGIN","userId":2}\n
응답: {"ok":true,"data":{"id":2,"name":"홍길동","role":"member","balance":50000}}\n
실패: {"ok":false,"error":"등록되지 않은 사용자입니다."}\n
```

## 사진
- `image` 필드는 **Base64 문자열**(줄바꿈 없음, Android: `Base64.NO_WRAP`)입니다.
- JPEG/PNG 모두 가능하고, 디코딩 후 **최대 5MB**입니다. 앱에서 **긴 변 600px 이하로 줄여서** 보내는 것을 권장합니다.
- 사진이 없는 상품은 `"image":""`입니다.
- 앱에서 추가하거나 바꾼 사진은 DB에 저장됩니다. 키오스크는 회원이 로그인할 때마다 상품 목록을 새로 받으므로 다음 손님부터 바로 반영됩니다.

## 명령

### LOGIN: 사용자 조회
```json
{"cmd":"LOGIN","userId":2}
→ {"ok":true,"data":{"id":2,"name":"홍길동","role":"member","balance":50000}}
```
`role`: `admin` 또는 `member`. `balance`: 충전 잔액(원).

### PRODUCTS: 상품 목록
```json
{"cmd":"PRODUCTS"}
→ {"ok":true,"data":[{"id":1,"name":"아메리카노","price":3000,"image":"iVBORw0KG..."}, ...]}
```

### ADD_PRODUCT: 상품 추가
```json
{"cmd":"ADD_PRODUCT","name":"녹차라떼","price":4000,"image":"/9j/4AAQ..."}
→ {"ok":true,"data":null}
```
`image`는 생략할 수 있습니다. 상품명이 이미 있으면(대소문자 무시) 실패합니다.

### UPDATE_PRODUCT: 상품 수정 (사진 변경 등)
```json
{"cmd":"UPDATE_PRODUCT","productId":1,"image":"/9j/4AAQ..."}
{"cmd":"UPDATE_PRODUCT","productId":1,"name":"아이스 아메리카노","price":3500}
→ {"ok":true,"data":null}
```
보낸 항목(`name`, `price`, `image`)만 바뀝니다. `"image":""`를 보내면 사진이 삭제됩니다.

### DELETE_PRODUCT: 상품 삭제
```json
{"cmd":"DELETE_PRODUCT","productId":3}
→ {"ok":true,"data":null}
```
이미 발생한 매출 기록은 유지됩니다.

### PAY: 결제
```json
{"cmd":"PAY","userId":2,"method":"face","items":[{"productId":1,"qty":2},{"productId":3,"qty":1}]}
→ {"ok":true,"data":{"total":11000,"balance":39000}}
```
- `method`: `card` 또는 `face`. `face`는 충전 잔액에서 차감하며, 잔액이 부족하면 실패합니다.
- `balance`는 `face` 결제일 때만 들어 있습니다.
- 금액은 서버가 DB 가격으로 계산합니다.

### SALES: 매출 조회
```json
{"cmd":"SALES","unit":"day","date":"2026-09-29"}
{"cmd":"SALES","unit":"month","date":"2026-09"}
→ {"ok":true,"data":{
     "buckets":[{"bucket":9,"card":6000,"face":3500}, ...],
     "details":[{"soldAt":"2026-09-29 09:12:03","buyer":"홍길동","product":"아메리카노",
                 "qty":2,"amount":6000,"method":"card"}, ...]}}
```
- `bucket`: `day`면 시(0~23), `month`면 일(1~31)입니다. 매출이 없는 구간은 목록에 없습니다.
- `details`: 최신순 구매 내역입니다.

## 참고
현재 서버에는 인증이 없어, 9000번 포트에 접속할 수 있으면 누구나 상품을 추가하거나 삭제할 수 있습니다. 매장 내부망에서만 쓰거나, 외부에 공개하기 전에 관리자 인증을 추가해야 합니다.
