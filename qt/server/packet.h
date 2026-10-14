#ifndef PACKET_H
#define PACKET_H

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

const quint16 PORT = 9000;
const quint16 ESP_PORT = 8000;
const int MAX_SIZE = 48 * 1024 * 1024;   // 회원가입 사진 5장(장당 최대 5MB, Base64 로 4/3배)이 한 줄에 들어가도록
const int SHELF_COUNT = 4;

inline QByteArray pack(const QJsonObject &obj)
{
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

inline bool unpack(QByteArray &buf, QJsonObject *obj)
{
    int n = buf.indexOf('\n');
    if (n < 0)
        return false;
    QByteArray line = buf.left(n);
    buf.remove(0, n + 1);
    *obj = QJsonDocument::fromJson(line).object();
    return true;
}

#endif
