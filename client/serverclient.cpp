#include "serverclient.h"
#include "protocol.h"

#include <QTcpSocket>

static QString g_host = "127.0.0.1";

void ServerClient::setHost(const QString &host)
{
    g_host = host;
}

bool ServerClient::call(const QString &cmd, const QVariantMap &args, QVariant *result, QString *err)
{
    QTcpSocket socket;
    socket.connectToHost(g_host, KIOSK_PORT);
    if (!socket.waitForConnected(3000)) {
        *err = QString("서버(%1:%2)에 연결할 수 없습니다.\n%3").arg(g_host).arg(KIOSK_PORT).arg(socket.errorString());
        return false;
    }
    QJsonObject request = QJsonObject::fromVariantMap(args);
    request["cmd"] = cmd;
    socket.write(packMessage(request));

    QByteArray buffer;
    QJsonObject response;
    while (!unpackMessage(buffer, &response)) {
        if (!socket.waitForReadyRead(10000)) {
            *err = "서버 응답이 없습니다.";
            return false;
        }
        buffer += socket.readAll();
    }

    if (!response.value("ok").toBool()) {
        *err = response.value("error").toString("서버 응답 형식이 잘못되었습니다.");
        return false;
    }
    if (result)
        *result = response.value("data").toVariant();
    return true;
}
