#include "server.h"
#include "db.h"
#include "packet.h"
#include "ai.h"
#include "forecast.h"

#include <QDate>
#include <QJsonDocument>
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

static const int HISTORY = 28;

static QString toJson(const QVariant &v)
{
    return QString::fromUtf8(QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact));
}

static bool predictProduct(DB *db, int productId, int days, QVector<int> *past, Forecast *f, QString *err)
{
    QDate from = QDate::currentDate().addDays(-HISTORY);
    if (!db->daily(productId, from, HISTORY, past, err))
        return false;
    *f = predict(*past, from.dayOfWeek(), days);
    return true;
}

static bool stockRows(DB *db, int productId, int days, QVariantList *rows, QString *err)
{
    QVariant list;
    if (!db->products(&list, err))
        return false;
    foreach (const QVariant &v, list.toList()) {
        QVariantMap p = v.toMap();
        int id = p.value("id").toInt();
        if (productId && id != productId)
            continue;
        QVector<int> past;
        Forecast f;
        if (!predictProduct(db, id, days, &past, &f, err))
            return false;
        int stock = p.value("stock").toInt();
        double avg = 0, predicted = 0;
        foreach (int q, past)
            avg += q;
        foreach (double q, f.qty)
            predicted += q;
        QVariantMap r;
        r["name"] = p.value("name");
        r["stock"] = stock;
        r["avg"] = avg / HISTORY;
        r["predicted"] = predicted;
        r["daysLeft"] = daysLeft(stock, f);
        r["order"] = orderQty(stock, f, days);
        *rows << r;
    }
    return true;
}

static bool aiForecast(DB *db, int productId, int days, QVariant *out, QString *err)
{
    if (days <= 0 || days > 31)
        days = 7;
    QVector<int> past;
    Forecast f;
    if (!predictProduct(db, productId, days, &past, &f, err))
        return false;

    QDate today = QDate::currentDate();
    QVariantList pastList, futureList;
    for (int i = HISTORY - 14; i < HISTORY; ++i) {
        QVariantMap m;
        m["label"] = today.addDays(i - HISTORY).toString("M/d");
        m["qty"] = past[i];
        pastList << m;
    }
    for (int i = 0; i < days; ++i) {
        QVariantMap m;
        m["label"] = today.addDays(i).toString("M/d");
        m["qty"] = f.qty[i];
        m["low"] = f.low[i];
        m["high"] = f.high[i];
        futureList << m;
    }
    QVariantList rows;
    if (!stockRows(db, productId, days, &rows, err))
        return false;

    QVariantMap res;
    res["past"] = pastList;
    res["future"] = futureList;
    res["rows"] = rows;
    *out = res;
    return true;
}

static bool aiReport(DB *db, const QString &period, QVariant *out, QString *err)
{
    QDate today = QDate::currentDate();
    QDate from = today, prev = today.addDays(-1);
    QString name = "오늘";
    if (period == "week") {
        from = today.addDays(1 - today.dayOfWeek());
        prev = from.addDays(-7);
        name = "이번 주";
    } else if (period == "month") {
        from = QDate(today.year(), today.month(), 1);
        prev = from.addMonths(-1);
        name = "이번 달";
    }
    QDateTime now = QDateTime::currentDateTime();
    QDateTime prevTo = QDateTime(prev).addSecs(QDateTime(from).secsTo(now));

    QVariantMap cur, old;
    QVariantList rows;
    if (!db->summary(QDateTime(from), now, &cur, err) || !db->summary(QDateTime(prev), prevTo, &old, err)
        || !stockRows(db, 0, 7, &rows, err))
        return false;

    QVariantMap data;
    data["기간"] = name;
    data["현재"] = cur;
    data["이전 같은 기간"] = old;
    data["재고 예측(7일)"] = rows;

    QString system = "당신은 무인 키오스크 매장의 매출 분석가입니다. "
                     "주어진 JSON 데이터의 숫자만 사용하고, 데이터에 없는 내용은 추측하지 마세요. "
                     "한국어로 '요약'(3줄), '잘한 점', '개선 제안'(2개), '재고 알림' 순서로 짧게 작성하세요. "
                     "제목은 <h3>, 문단은 <p>, 목록은 <ul><li>, 강조는 <b> 태그만 쓴 HTML 조각으로만 답하고 "
                     "```나 <html> 태그는 쓰지 마세요.";
    QString answer;
    if (!AI::ask(system, name + " 매출 리포트를 작성해 주세요.\n\n" + toJson(data), QJsonObject(), &answer, err))
        return false;
    *out = answer;
    return true;
}

static bool aiAsk(DB *db, const QString &question, int productId, int userId, QVariant *out, QString *err)
{
    QDate today = QDate::currentDate();
    QDate week = today.addDays(1 - today.dayOfWeek());
    QDateTime now = QDateTime::currentDateTime();

    QVariantMap todayData, weekData, lastWeek, month;
    QVariantList rows;
    if (!db->summary(QDateTime(today), now, &todayData, err)
        || !db->summary(QDateTime(week), now, &weekData, err)
        || !db->summary(QDateTime(week.addDays(-7)), QDateTime(week), &lastWeek, err)
        || !db->summary(QDateTime(today.addDays(-30)), now, &month, err)
        || !stockRows(db, 0, 7, &rows, err))
        return false;

    QVariantMap data;
    data["오늘"] = todayData;
    data["이번 주"] = weekData;
    data["지난주"] = lastWeek;
    data["최근 30일"] = month;
    data["재고 예측(7일)"] = rows;
    if (productId) {
        QVector<int> past;
        Forecast f;
        if (!predictProduct(db, productId, 7, &past, &f, err))
            return false;
        QVariantList list;
        foreach (int q, past)
            list << q;
        data["선택 상품 최근 28일 일별 판매"] = list;
    }
    if (userId) {
        QVariantList list;
        if (!db->purchases(userId, 90, &list, err))
            return false;
        data["선택 회원 최근 90일 구매"] = list;
    }

    QJsonObject schema = QJsonDocument::fromJson(
        "{\"type\":\"object\",\"properties\":{"
        "\"answer\":{\"type\":\"string\"},"
        "\"followUps\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},"
        "\"required\":[\"answer\",\"followUps\"],\"additionalProperties\":false}").object();
    QString system = "당신은 무인 키오스크 매장의 매출 상담 AI입니다. "
                     "주어진 JSON 데이터의 숫자만 근거로 답하고, 데이터에 없으면 없다고 말하세요. "
                     "answer는 마크다운 없이 5문장 이내의 한국어로 쓰고, "
                     "followUps에는 이어서 물어볼 만한 20자 이내의 질문 3개를 넣으세요.";
    QString answer;
    if (!AI::ask(system, "질문: " + question + "\n\n데이터:\n" + toJson(data), schema, &answer, err))
        return false;
    *out = QJsonDocument::fromJson(answer.toUtf8()).object().toVariantMap();
    return true;
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
                       req.value("cardUid").toString(), req.value("items").toArray().toVariantList(),
                       &data, &err);
    } else if (cmd == "SALES") {
        ok = m_db->sales(req.value("unit").toString(), req.value("date").toString(), &data, &err);
    } else if (cmd == "MEMBERS") {
        ok = m_db->members(&data, &err);
    } else if (cmd == "AI_REPORT") {
        ok = aiReport(m_db, req.value("period").toString(), &data, &err);
    } else if (cmd == "AI_ASK") {
        ok = aiAsk(m_db, req.value("question").toString(), req.value("productId").toInt(),
                   req.value("userId").toInt(), &data, &err);
    } else if (cmd == "AI_FORECAST") {
        ok = aiForecast(m_db, req.value("productId").toInt(), req.value("days").toInt(), &data, &err);
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
    AI::setup(ini);

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

void Esp::handle(const QString &line)
{
    QString cmd = line.section(':', 0, 0).trimmed().toLower();
    QString val = line.section(':', 1).trimmed().toLower();
    int n = cmd.startsWith("stand") ? cmd.mid(5).trimmed().toInt() : 0;
    if (cmd == "person") {
        m_present = val == "1";
        emit presence(m_present);
    } else if (cmd == "card" && !val.isEmpty()) {
        emit card(val.remove(' ').toUpper());
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

void Esp::cardSelect()
{
    send("CARD_SELECT");
}

void Esp::cardFail()
{
    send("CARD_FAIL");
}

void Esp::cardCancel()
{
    send("CARD_CANCEL");
}
