#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

// 키오스크 / 안드로이드 앱 <-> 서버 통신 포트
const quint16 KIOSK_PORT = 9000;

// 한 메시지의 최대 크기 (사진 Base64 포함)
const int MAX_MESSAGE_SIZE = 16 * 1024 * 1024;

// 메시지 형식: UTF-8 JSON 객체 한 줄 + '\n'
//   요청: {"cmd": "LOGIN", "userId": 2}
//   응답: {"ok": true, "data": ...}  또는  {"ok": false, "error": "사유"}

inline QByteArray packMessage(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

// buffer에 한 줄이 완성돼 있으면 꺼내서 obj에 담고 true를 반환한다.
// JSON이 아니거나 객체가 아니면 obj는 빈 객체가 된다.
inline bool unpackMessage(QByteArray &buffer, QJsonObject *obj)
{
    const int newline = buffer.indexOf('\n');
    if (newline < 0)
        return false;
    const QByteArray line = buffer.left(newline);
    buffer.remove(0, newline + 1);
    *obj = QJsonDocument::fromJson(line).object();
    return true;
}

#endif // PROTOCOL_H
