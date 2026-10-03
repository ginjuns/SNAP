#include "esp.h"
#include "packet.h"

#include <QJsonArray>
#include <QTcpSocket>

static Esp *instance = 0;

Esp::Esp(QObject *parent)
    : QTcpServer(parent), m_sock(0), m_present(false)
{
    instance = this;
    connect(this, SIGNAL(newConnection()), SLOT(onConnect()));
}

Esp::~Esp()
{
    if (instance == this)
        instance = 0;
}

Esp *Esp::get()
{
    return instance;
}

bool Esp::start(QString *err)
{
    if (listen(QHostAddress::Any, ESP_PORT))
        return true;
    *err = QString("ESP32 포트 %1 열기 실패: %2").arg(ESP_PORT).arg(errorString());
    return false;
}

bool Esp::connected() const
{
    return m_sock && m_sock->state() == QAbstractSocket::ConnectedState;
}

void Esp::onConnect()
{
    while (QTcpSocket *sock = nextPendingConnection()) {
        if (m_sock)
            m_sock->disconnectFromHost();
        m_sock = sock;
        m_buf.clear();
        connect(sock, SIGNAL(readyRead()), SLOT(onRead()));
        connect(sock, SIGNAL(disconnected()), SLOT(onClose()));
        qDebug("[ESP32] 연결됨 %s", qPrintable(sock->peerAddress().toString()));
    }
}

void Esp::onRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (sock != m_sock) {
        sock->readAll();
        return;
    }
    m_buf += sock->readAll();

    QJsonObject req;
    while (unpack(m_buf, &req))
        handle(req);

    if (m_buf.size() > MAX_SIZE) {
        m_buf.clear();
        sock->disconnectFromHost();
    }
}

void Esp::onClose()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    if (sock == m_sock) {
        m_sock = 0;
        m_buf.clear();
        qDebug("[ESP32] 연결 끊김");
    }
    sock->deleteLater();
}

void Esp::handle(const QJsonObject &req)
{
    QString cmd = req.value("cmd").toString();
    if (cmd == "PRESENCE") {
        m_present = req.value("present").toBool();
        emit presence(m_present);
    } else if (cmd == "CARD") {
        emit card(req.value("uid").toString());
    } else if (cmd == "SHELF") {
        m_shelf = req.value("shelf").toArray().toVariantList();
        emit shelfChanged(m_shelf);
    } else {
        qDebug("[ESP32] 알 수 없는 명령: %s", qPrintable(cmd));
        return;
    }
    qDebug("[ESP32] %s", QJsonDocument(req).toJson(QJsonDocument::Compact).constData());
}

void Esp::send(const QJsonObject &obj)
{
    if (!connected()) {
        qDebug("[ESP32] 연결 안 됨, 보내지 못함: %s", qPrintable(obj.value("cmd").toString()));
        return;
    }
    m_sock->write(pack(obj));
}

void Esp::buzzer()
{
    QJsonObject obj;
    obj["cmd"] = "BUZZER";
    obj["sound"] = "success";
    send(obj);
}
