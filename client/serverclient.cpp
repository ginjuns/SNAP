#include "serverclient.h"
#include "protocol.h"

#include <QTcpSocket>

static QString g_host = "127.0.0.1";

void ServerClient::setHost(const QString &host)
{
    g_host = host;
}

bool ServerClient::call(const QString &cmd, const QVariantList &args, QVariant *result, QString *err)
{
    QTcpSocket socket;
    socket.connectToHost(g_host, KIOSK_PORT);
    if (!socket.waitForConnected(3000)) {
        *err = QString("서버(%1:%2)에 연결할 수 없습니다.\n%3").arg(g_host).arg(KIOSK_PORT).arg(socket.errorString());
        return false;
    }
    socket.write(packMessage(QVariantList() << cmd << args));

    QByteArray buffer;
    QVariantList response;
    while (!unpackMessage(buffer, &response)) {
        if (!socket.waitForReadyRead(5000)) {
            *err = "서버 응답이 없습니다.";
            return false;
        }
        buffer += socket.readAll();
    }

    if (!response.value(0).toBool()) {
        *err = response.value(1).toString();
        return false;
    }
    if (result)
        *result = response.value(1);
    return true;
}
