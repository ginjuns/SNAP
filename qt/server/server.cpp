#include "server.h"
#include "db.h"
#include "packet.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QSettings>
#include <QStringList>
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
                                 req.value("stock").toInt(), req.value("shelf").toInt(), img, &err);
    } else if (cmd == "UPDATE_PRODUCT") {
        QVariantMap f;
        if (req.contains("name"))
            f["name"] = req.value("name").toString();
        if (req.contains("price"))
            f["price"] = req.value("price").toInt();
        if (req.contains("stock"))
            f["stock"] = req.value("stock").toInt();
        if (req.contains("shelf"))
            f["shelf"] = req.value("shelf").toInt();
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

static QString findIni(QStringList *tried)
{
    QStringList dirs;
    dirs << QDir::currentPath() << QCoreApplication::applicationDirPath() << QString(SERVER_SOURCE_DIR);
    foreach (const QString &dir, dirs) {
        QString path = QDir(dir).absoluteFilePath("server.ini");
        if (QFileInfo(path).isFile())
            return path;
        if (!tried->contains(path))
            *tried << path;
    }
    return QString();
}

ServerThread::~ServerThread()
{
    quit();
    wait();
}

bool ServerThread::startWait(QString *err)
{
    start();
    m_ready.acquire();
    *err = m_err;
    return m_ok;
}

void ServerThread::run()
{
    m_ok = false;
    QStringList tried;
    QString path = findIni(&tried);
    if (path.isEmpty()) {
        m_err = QString("server.ini 를 찾을 수 없습니다. server.ini.example 을 server.ini 로 복사하세요.\n찾아본 위치:\n  %1")
                    .arg(tried.join("\n  "));
        m_ready.release();
        return;
    }
    QSettings ini(path, QSettings::IniFormat);

    DB db;
    Server server(&db);
    QString dbErr;
    if (!db.open(ini, &dbErr)) {
        m_err = "DB 연결 실패: " + dbErr;
    } else if (!server.listen(QHostAddress::Any, PORT)) {
        m_err = QString("포트 %1 열기 실패: %2").arg(PORT).arg(server.errorString());
    } else {
        m_ok = true;
        qDebug("키오스크 서버 시작 (포트 %d, 설정 %s)", PORT, qPrintable(path));
    }
    m_ready.release();
    if (m_ok)
        exec();
}

static Esp *instance = 0;

Esp::Esp(QObject *parent)
    : QTcpServer(parent), m_sock(0), m_present(false)
{
    instance = this;
    for (int i = 0; i < SHELF_COUNT; ++i)
        m_shelf << 1;   // 아직 소식이 없는 진열대는 차 있다고 본다
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

// 진열대가 비었는지. 지정 안 한 상품(0)이면 false
bool Esp::shelfEmpty(int shelf)
{
    if (!instance || shelf < 1 || shelf > instance->m_shelf.size())
        return false;
    return instance->m_shelf.at(shelf - 1).toInt() == 0;
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

    int n;
    while ((n = m_buf.indexOf('\n')) >= 0) {
        QString line = QString::fromUtf8(m_buf.left(n)).trimmed();
        m_buf.remove(0, n + 1);
        if (!line.isEmpty())
            handle(line);
    }

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

// 단어 명령 한 줄: "person:1", "card:ok", "stand 1:1" (대소문자 무시)
void Esp::handle(const QString &line)
{
    QString cmd = line.section(':', 0, 0).trimmed().toLower();
    QString val = line.section(':', 1).trimmed().toLower();
    int n = cmd.startsWith("stand") ? cmd.mid(5).trimmed().toInt() : 0;
    if (cmd == "person") {
        m_present = val == "1";
        emit presence(m_present);
    } else if (cmd == "card" && val == "ok") {
        emit card();
    } else if (n >= 1 && n <= SHELF_COUNT) {
        m_shelf[n - 1] = val == "1" ? 1 : 0;
        emit shelfChanged(m_shelf);
    } else {
        qDebug("[ESP32] 알 수 없는 명령: %s", qPrintable(line));
        return;
    }
    qDebug("[ESP32] %s", qPrintable(line));
}

void Esp::send(const QString &cmd)
{
    if (!connected()) {
        qDebug("[ESP32] 연결 안 됨, 보내지 못함: %s", qPrintable(cmd));
        return;
    }
    m_sock->write(cmd.toUtf8() + '\n');
}

void Esp::buzzer()
{
    send("BUZZER");
}
