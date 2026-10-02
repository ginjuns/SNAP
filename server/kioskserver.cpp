#include "kioskserver.h"
#include "database.h"
#include "protocol.h"

#include <QJsonArray>
#include <QTcpSocket>

static const int MAX_IMAGE_SIZE = 5 * 1024 * 1024;   // 디코딩 후 사진 최대 5MB

// 요청의 "image"(Base64)를 꺼낸다. 없거나 ""이면 빈 값.
static bool decodeImage(const QJsonObject &req, QByteArray *image, QString *err)
{
    *image = QByteArray::fromBase64(req.value("image").toString().toLatin1());
    if (image->size() > MAX_IMAGE_SIZE) {
        *err = "사진이 너무 큽니다. (최대 5MB, 앱에서 줄여서 보내 주세요)";
        return false;
    }
    return true;
}

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

    QJsonObject request;
    while (unpackMessage(buffer, &request))
        socket->write(packMessage(handle(request)));

    if (buffer.size() > MAX_MESSAGE_SIZE) {   // 줄바꿈 없이 너무 큰 데이터 -> 연결 종료
        QJsonObject res;
        res["ok"] = false;
        res["error"] = "메시지가 너무 큽니다.";
        buffer.clear();
        socket->write(packMessage(res));
        socket->disconnectFromHost();
    }
}

void KioskServer::onDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    m_buffers.remove(socket);
    socket->deleteLater();
}

QJsonObject KioskServer::handle(const QJsonObject &req)
{
    const QString cmd = req.value("cmd").toString();
    QVariant data;
    QString err;
    bool ok = false;

    if (cmd == "LOGIN") {
        ok = m_db->user(req.value("userId").toInt(), &data, &err);
    } else if (cmd == "PASSWORD_LOGIN") {
        ok = m_db->userByPassword(req.value("loginId").toString(), req.value("password").toString(), &data, &err);
    } else if (cmd == "REGISTER") {
        QList<QByteArray> faces;
        ok = true;
        foreach (const QJsonValue &v, req.value("faces").toArray()) {
            QJsonObject one;
            one["image"] = v;
            QByteArray image;
            if (!(ok = decodeImage(one, &image, &err)))
                break;
            if (!image.isEmpty())
                faces << image;
        }
        ok = ok && m_db->registerUser(req.value("name").toString(), req.value("loginId").toString(),
                                      req.value("password").toString(), faces, &data, &err);
    } else if (cmd == "FACES") {
        ok = m_db->faces(req.value("afterId").toInt(), &data, &err);
    } else if (cmd == "PRODUCTS") {
        ok = m_db->products(&data, &err);
    } else if (cmd == "ADD_PRODUCT") {
        QByteArray image;
        ok = decodeImage(req, &image, &err)
             && m_db->addProduct(req.value("name").toString(), req.value("price").toInt(),
                                 req.value("stock").toInt(), image, &err);
    } else if (cmd == "UPDATE_PRODUCT") {
        QVariantMap fields;
        if (req.contains("name"))
            fields["name"] = req.value("name").toString();
        if (req.contains("price"))
            fields["price"] = req.value("price").toInt();
        if (req.contains("stock"))
            fields["stock"] = req.value("stock").toInt();
        QByteArray image;
        ok = decodeImage(req, &image, &err);
        if (ok) {
            if (req.contains("image"))
                fields["image"] = image;
            ok = m_db->updateProduct(req.value("productId").toInt(), fields, &err);
        }
    } else if (cmd == "DELETE_PRODUCT") {
        ok = m_db->deleteProduct(req.value("productId").toInt(), &err);
    } else if (cmd == "PAY") {
        ok = m_db->pay(req.value("userId").toInt(), req.value("method").toString(),
                       req.value("items").toArray().toVariantList(), &data, &err);
    } else if (cmd == "SALES") {
        ok = m_db->sales(req.value("unit").toString(), req.value("date").toString(), &data, &err);
    } else {
        err = cmd.isEmpty() ? "요청 형식이 잘못되었습니다. (cmd가 있는 JSON 객체 한 줄)"
                            : "알 수 없는 명령: " + cmd;
    }

    qDebug("[%s] %s %s", qPrintable(cmd), ok ? "OK" : "FAIL", qPrintable(err));

    QJsonObject res;
    res["ok"] = ok;
    if (ok)
        res["data"] = QJsonValue::fromVariant(data);
    else
        res["error"] = err;
    return res;
}
