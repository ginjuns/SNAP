#include "kioskserver.h"
#include "database.h"
#include "protocol.h"

#include <QTcpSocket>

KioskServer::KioskServer(Database *db, QObject *parent)
    : QTcpServer(parent), m_db(db)
{
    connect(this, SIGNAL(newConnection()), SLOT(onNewConnection()));
}

void KioskServer::onNewConnection()
{
    while (QTcpSocket *socket = nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, SIGNAL(readyRead()), SLOT(onReadyRead()));
        connect(socket, SIGNAL(disconnected()), SLOT(onDisconnected()));
    }
}

void KioskServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();

    QVariantList request;
    while (unpackMessage(buffer, &request))
        socket->write(packMessage(handle(request)));
}

void KioskServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    m_buffers.remove(socket);
    socket->deleteLater();
}

QVariantList KioskServer::handle(const QVariantList &req)
{
    const QString cmd = req.value(0).toString();
    QVariant data;
    QString err;
    bool ok = false;

    if (cmd == "LOGIN")
        ok = m_db->user(req.value(1).toInt(), &data, &err);
    else if (cmd == "PRODUCTS")
        ok = m_db->products(&data, &err);
    else if (cmd == "ADD_PRODUCT")
        ok = m_db->addProduct(req.value(1).toString(), req.value(2).toInt(), req.value(3).toByteArray(), &err);
    else if (cmd == "DELETE_PRODUCT")
        ok = m_db->deleteProduct(req.value(1).toInt(), &err);
    else if (cmd == "PAY")
        ok = m_db->pay(req.value(1).toInt(), req.value(2).toString(), req.value(3).toList(), &data, &err);
    else if (cmd == "SALES")
        ok = m_db->sales(req.value(1).toString(), req.value(2).toString(), &data, &err);
    else
        err = "알 수 없는 명령: " + cmd;

    qDebug("[%s] %s %s", qPrintable(cmd), ok ? "OK" : "FAIL", qPrintable(err));
    return QVariantList() << ok << (ok ? data : QVariant(err));
}
