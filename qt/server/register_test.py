#!/usr/bin/env python3
# 안드로이드 앱 대신 REGISTER(얼굴 사진 회원가입)를 보내 보는 테스트 도구.
# 사용법: python3 qt/server/register_test.py 이름 아이디 비밀번호 사진1.jpg 사진2.jpg ...
#         (다른 PC의 서버면 앞에 KIOSK_HOST=192.168.0.10 을 붙인다)
import base64, json, os, socket, sys

if len(sys.argv) < 5:
    sys.exit("사용법: python3 qt/server/register_test.py 이름 아이디 비밀번호 사진1.jpg 사진2.jpg ...")

name, login_id, password, photos = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
faces = [base64.b64encode(open(p, "rb").read()).decode() for p in photos]
request = {"cmd": "REGISTER", "name": name, "loginId": login_id, "password": password, "faces": faces}

with socket.create_connection((os.environ.get("KIOSK_HOST", "127.0.0.1"), 9000), timeout=10) as s:
    s.sendall(json.dumps(request).encode() + b"\n")
    response = s.makefile("rb").readline()
print(json.loads(response))
