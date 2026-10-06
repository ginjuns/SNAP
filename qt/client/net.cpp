#include "net.h"
#include "packet.h"

#include <QTcpSocket>

static QString host = "127.0.0.1";

void Net::setHost(const QString &ip)
{
    host = ip;
}

bool Net::call(const QString &cmd, const QVariantMap &args, QVariant *out, QString *err)
{
    QTcpSocket sock;
    sock.connectToHost(host, PORT);
    if (!sock.waitForConnected(3000)) {
        *err = QString("서버(%1:%2)에 연결할 수 없습니다.\n%3").arg(host).arg(PORT).arg(sock.errorString());
        return false;
    }
    QJsonObject req = QJsonObject::fromVariantMap(args);
    req["cmd"] = cmd;
    sock.write(pack(req));

    QByteArray buf;
    QJsonObject res;
    while (!unpack(buf, &res)) {
        if (!sock.waitForReadyRead(cmd.startsWith("AI_") ? 90000 : 10000)) {
            *err = "서버 응답이 없습니다.";
            return false;
        }
        buf += sock.readAll();
    }

    if (!res.value("ok").toBool()) {
        *err = res.value("error").toString("서버 응답 형식이 잘못되었습니다.");
        return false;
    }
    if (out)
        *out = res.value("data").toVariant();
    return true;
}
