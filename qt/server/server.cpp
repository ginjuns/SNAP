#include "server.h"
#include "db.h"
#include "packet.h"

#include <QJsonArray>
#include <QTcpSocket>

static const int MAX_IMG = 5 * 1024 * 1024;

static bool readImage(const QJsonObject &req, QByteArray *img, QString *err)
{
    *img = QByteArray::fromBase64(req.value("image").toString().toLatin1());
    if (img->size() > MAX_IMG) {
        *err = "사진이 너무 큽니다. (최대 5MB, 앱에서 줄여서 보내 주세요)";
        return false;
    }
    return true;
}

Server::Server(DB *db, QObject *parent)
    : QTcpServer(parent), m_db(db)
{
    connect(this, SIGNAL(newConnection()), SLOT(onConnect()));
}

void Server::onConnect()
{
    while (QTcpSocket *sock = nextPendingConnection()) {
        m_buf.insert(sock, QByteArray());
        connect(sock, SIGNAL(readyRead()), SLOT(onRead()));
        connect(sock, SIGNAL(disconnected()), SLOT(onClose()));
    }
}

void Server::onRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    QByteArray &buf = m_buf[sock];
    buf += sock->readAll();

    QJsonObject req;
    while (unpack(buf, &req))
        sock->write(pack(handle(req)));

    if (buf.size() > MAX_SIZE) {
        QJsonObject res;
        res["ok"] = false;
        res["error"] = "메시지가 너무 큽니다.";
        buf.clear();
        sock->write(pack(res));
        sock->disconnectFromHost();
    }
}

void Server::onClose()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    m_buf.remove(sock);
    sock->deleteLater();
}

QJsonObject Server::handle(const QJsonObject &req)
{
    QString cmd = req.value("cmd").toString();
    QVariant data;
    QString err;
    bool ok = false;

    if (cmd == "LOGIN") {
        ok = m_db->user(req.value("userId").toInt(), &data, &err);
    } else if (cmd == "PASSWORD_LOGIN") {
        ok = m_db->login(req.value("loginId").toString(), req.value("password").toString(), &data, &err);
    } else if (cmd == "REGISTER") {
        QList<QByteArray> faces;
        ok = true;
        foreach (const QJsonValue &v, req.value("faces").toArray()) {
            QJsonObject one;
            one["image"] = v;
            QByteArray img;
            if (!(ok = readImage(one, &img, &err)))
                break;
            if (!img.isEmpty())
                faces << img;
        }
        ok = ok && m_db->join(req.value("name").toString(), req.value("loginId").toString(),
                              req.value("password").toString(), faces, &data, &err);
    } else if (cmd == "FACES") {
        ok = m_db->faces(req.value("afterId").toInt(), &data, &err);
    } else if (cmd == "PRODUCTS") {
        ok = m_db->products(&data, &err);
    } else if (cmd == "ADD_PRODUCT") {
        QByteArray img;
        ok = readImage(req, &img, &err)
             && m_db->addProduct(req.value("name").toString(), req.value("price").toInt(),
                                 req.value("stock").toInt(), img, &err);
    } else if (cmd == "UPDATE_PRODUCT") {
        QVariantMap f;
        if (req.contains("name"))
            f["name"] = req.value("name").toString();
        if (req.contains("price"))
            f["price"] = req.value("price").toInt();
        if (req.contains("stock"))
            f["stock"] = req.value("stock").toInt();
        QByteArray img;
        ok = readImage(req, &img, &err);
        if (ok) {
            if (req.contains("image"))
                f["image"] = img;
            ok = m_db->editProduct(req.value("productId").toInt(), f, &err);
        }
    } else if (cmd == "DELETE_PRODUCT") {
        ok = m_db->delProduct(req.value("productId").toInt(), &err);
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
