#ifndef PACKET_H
#define PACKET_H

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

const quint16 PORT = 9000;
const int MAX_SIZE = 16 * 1024 * 1024;

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
