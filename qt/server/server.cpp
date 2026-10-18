#include "server.h"
#include "db.h"
#include "packet.h"
#include "ai.h"
#include "forecast.h"

#include <QBuffer>
#include <QDate>
#include <QImageReader>
#include <QUuid>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

static const int MAX_IMG = 5 * 1024 * 1024;

// 요청의 image(Base64)를 바이트로 복원, 5MB 가 넘으면 오류
static bool readImage(const QJsonObject &req, QByteArray *img, QString *err)
{
    *img = QByteArray::fromBase64(req.value("image").toString().toLatin1());
    if (img->size() > MAX_IMG) {
        *err = "사진이 너무 큽니다. (최대 5MB, 앱에서 줄여서 보내 주세요)";
        return false;
    }
    return true;
}

// 요청의 숫자 값 읽기. 앱이 "3000" 처럼 문자열로 보내도 숫자로 바꿈
static int num(const QJsonObject &req, const QString &key)
{
    QJsonValue v = req.value(key);
    return v.isString() ? v.toString().trimmed().toInt() : v.toInt();
}

// 요청에 그 항목이 값과 함께 들어 있는지 (없거나 null 이면 false)
static bool has(const QJsonObject &req, const QString &key)
{
    QJsonValue v = req.value(key);
    return !v.isUndefined() && !v.isNull();
}

// 앱/화면 요청을 받는 서버 생성
Server::Server(DB *db, QObject *parent)
    : QTcpServer(parent), m_db(db)
{
    connect(this, SIGNAL(newConnection()), SLOT(onConnect()));
}

// 새 접속을 받아 수신 버퍼를 만들고 읽기/끊김 신호 연결
void Server::onConnect()
{
    while (QTcpSocket *sock = nextPendingConnection()) {
        m_buf.insert(sock, QByteArray());
        connect(sock, SIGNAL(readyRead()), SLOT(onRead()));
        connect(sock, SIGNAL(disconnected()), SLOT(onClose()));
    }
}

// 받은 데이터에서 한 줄씩 요청을 꺼내 처리하고 응답, 너무 크면 연결 끊기
// 토스·AI 응답을 기다리는 동안 다른 이벤트가 처리되므로, 이 연결은 다시 읽지 않고(끝난 뒤 이어서 읽음)
// 그 사이 접속이 끊겼으면 응답하지 않음
void Server::onRead()
{
    QTcpSocket *raw = qobject_cast<QTcpSocket *>(sender());
    if (m_busy.contains(raw))
        return;
    m_busy.insert(raw);
    QPointer<QTcpSocket> sock = raw;
    bool local = sock->peerAddress().isLoopback();
    for (;;) {
        QByteArray &buf = m_buf[sock];
        buf += sock->readAll();
        QJsonObject req;
        if (!unpack(buf, &req)) {
            if (buf.size() > MAX_SIZE) {
                QJsonObject res;
                res["ok"] = false;
                res["error"] = "메시지가 너무 큽니다.";
                buf.clear();
                sock->write(pack(res));
                sock->disconnectFromHost();
            }
            break;
        }
        QJsonObject res = handle(req, local);
        if (!sock || !m_buf.contains(sock))
            break;
        sock->write(pack(res));
    }
    m_busy.remove(raw);
}

// 접속이 끊기면 버퍼 정리
void Server::onClose()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket *>(sender());
    m_buf.remove(sock);
    sock->deleteLater();
}

static const int HISTORY = 28;

// AI 에 넘길 데이터를 JSON 문자열로 변환
static QString toJson(const QVariant &v)
{
    return QString::fromUtf8(QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact));
}

// 상품(0 이면 전체)의 지난 28일 일별 판매량으로 앞으로 days 일 판매 예측
static bool predictProduct(DB *db, int productId, int days, QVector<int> *past, Forecast *f, QString *err)
{
    QDate from = QDate::currentDate().addDays(-HISTORY);
    if (!db->daily(productId, from, HISTORY, past, err))
        return false;
    *f = predict(*past, from.dayOfWeek(), days);
    return true;
}

// 상품마다 재고, 하루 평균 판매, 예측 판매량, 품절 예상일, 발주 추천 수량 계산
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

// AI_FORECAST: 그래프용 지난 14일/예측 데이터와 상품별 재고 예측 표
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

// 이전 기간 대비 변화율 문구 (+12% / -5% / 이전 0이면 "-")
static QString change(double cur, double old)
{
    if (old <= 0)
        return cur > 0 ? QString("신규") : QString("-");
    int pct = qRound((cur - old) * 100.0 / old);
    return QString("%1%2%").arg(pct > 0 ? "+" : "").arg(pct);
}

// 변화율 칸 HTML
static QString changeCell(double cur, double old)
{
    QString text = change(cur, old);
    QString color = text.startsWith('+') || text == "신규" ? "#1b7f3b" : text.startsWith('-') && text != "-" ? "#c62828" : "#888";
    return QString("<td align=right style='color:%1'>%2</td>").arg(color, text);
}

static QString won(int v)
{
    return QString("%L1원").arg(v);
}

// AI 답의 문장 목록을 <ul> 로
static QString bullets(const QJsonArray &list, bool numbered = false)
{
    if (list.isEmpty())
        return "<p style='color:#888'>없음</p>";
    QString html = numbered ? "<ol>" : "<ul>";
    foreach (const QJsonValue &v, list)
        html += "<li>" + v.toString().toHtmlEscaped() + "</li>";
    return html + (numbered ? "</ol>" : "</ul>");
}

// AI_REPORT: 고른 날짜의 일/주/월 매출 리포트
static bool aiReport(DB *db, const QString &unit, const QString &date, QVariant *out, QString *err)
{
    QDate d = QDate::fromString(date, "yyyy-MM-dd");
    if (!d.isValid())
        d = QDate::currentDate();
    QDate from = d, end = d.addDays(1), prev = d.addDays(-1);
    QString name = QLocale().toString(d, "yyyy년 M월 d일 (ddd)"), prevName = "전날";
    if (unit == "week") {
        from = d.addDays(1 - d.dayOfWeek());
        end = from.addDays(7);
        prev = from.addDays(-7);
        name = QString("%1 ~ %2 주간").arg(from.toString("yyyy년 M월 d일"), end.addDays(-1).toString("M월 d일"));
        prevName = "지난주";
    } else if (unit == "month") {
        from = QDate(d.year(), d.month(), 1);
        end = from.addMonths(1);
        prev = from.addMonths(-1);
        name = from.toString("yyyy년 M월");
        prevName = "지난달";
    }
    QDateTime now = QDateTime::currentDateTime();
    if (QDateTime(from) > now) {
        *err = "아직 오지 않은 기간은 리포트를 만들 수 없습니다.";
        return false;
    }
    bool partial = QDateTime(end) > now;
    QDateTime to = partial ? now : QDateTime(end);
    QDateTime prevTo = partial ? QDateTime(prev).addSecs(QDateTime(from).secsTo(now)) : QDateTime(from);
    if (partial)
        prevName += " 같은 시각까지";

    QVariantMap cur, old;
    QVariant productList;
    QVariantList stock;
    if (!db->summary(QDateTime(from), to, &cur, err) || !db->summary(QDateTime(prev), prevTo, &old, err)
        || !db->products(&productList, err) || !stockRows(db, 0, 7, &stock, err))
        return false;

    int total = cur.value("total").toInt(), oldTotal = old.value("total").toInt();
    int orders = cur.value("orders").toInt(), oldOrders = old.value("orders").toInt();
    int avg = orders ? total / orders : 0, oldAvg = oldOrders ? oldTotal / oldOrders : 0;

    QString html = QString("<h3>%1 매출 리포트</h3>").arg(name.toHtmlEscaped());
    html += QString("<p style='color:#888'>비교 기준: %1</p>").arg(prevName);
    html += "<table width=100% cellspacing=0 cellpadding=6 border=1 style='border-color:#a8dca8'>"
            "<tr style='background:#cdffcd'><th align=left>항목</th><th align=right>이번</th>"
            "<th align=right>" + prevName.section(' ', 0, 0) + "</th><th align=right>변화</th></tr>";
    struct Kpi { const char *label; int cur, old; bool money; };
    Kpi kpis[] = {
        {"매출", total, oldTotal, true},
        {"결제 건수", orders, oldOrders, false},
        {"객단가 (1건 평균)", avg, oldAvg, true},
        {"판매 수량", cur.value("qty").toInt(), old.value("qty").toInt(), false},
        {"구매 회원 수", cur.value("customers").toInt(), old.value("customers").toInt(), false},
        {"카드 매출", cur.value("card").toInt(), old.value("card").toInt(), true},
        {"얼굴인식 매출", cur.value("face").toInt(), old.value("face").toInt(), true},
    };
    for (const Kpi &k : kpis) {
        html += QString("<tr><td>%1</td><td align=right><b>%2</b></td><td align=right>%3</td>%4</tr>")
                    .arg(k.label)
                    .arg(k.money ? won(k.cur) : QString("%L1").arg(k.cur))
                    .arg(k.money ? won(k.old) : QString("%L1").arg(k.old))
                    .arg(changeCell(k.cur, k.old));
    }
    html += "</table>";
    QMap<QString, int> oldQty;
    foreach (const QVariant &v, old.value("products").toList())
        oldQty[v.toMap().value("name").toString()] = v.toMap().value("qty").toInt();
    QVariantList productRows;
    QSet<QString> sold;
    QString productHtml;
    foreach (const QVariant &v, cur.value("products").toList()) {
        QVariantMap m = v.toMap();
        QString pname = m.value("name").toString();
        int qty = m.value("qty").toInt();
        sold << pname;
        QVariantMap r;
        r["상품"] = pname;
        r["수량"] = qty;
        r["매출"] = m.value("amount");
        r["이전 수량"] = oldQty.value(pname);
        r["변화"] = change(qty, oldQty.value(pname));
        productRows << r;
        productHtml += QString("<tr><td>%1</td><td align=right>%L2개</td><td align=right>%3</td>"
                               "<td align=right>%L4개</td>%5</tr>")
                           .arg(pname.toHtmlEscaped()).arg(qty).arg(won(m.value("amount").toInt()))
                           .arg(oldQty.value(pname)).arg(changeCell(qty, oldQty.value(pname)));
    }
    QStringList unsold;
    foreach (const QVariant &v, productList.toList()) {
        QString pname = v.toMap().value("name").toString();
        if (!sold.contains(pname))
            unsold << pname;
    }

    QString peak;
    int peakAmount = 0;
    foreach (const QVariant &v, cur.value("hours").toList()) {
        QVariantMap m = v.toMap();
        if (m.value("amount").toInt() > peakAmount) {
            peakAmount = m.value("amount").toInt();
            peak = QString("%1시").arg(m.value("hour").toInt());
        }
    }

    QVariantMap data;
    data["기간"] = name;
    data["비교 기준"] = prevName;
    data["진행 중인 기간"] = partial;
    data["핵심 지표"] = QVariantMap{
        {"매출", total}, {"매출 변화", change(total, oldTotal)},
        {"결제 건수", orders}, {"결제 건수 변화", change(orders, oldOrders)},
        {"객단가", avg}, {"객단가 변화", change(avg, oldAvg)},
        {"카드 매출", cur.value("card")}, {"얼굴인식 매출", cur.value("face")},
        {"구매 회원 수", cur.value("customers")}, {"가장 바쁜 시간대", peak}};
    data["상품별 판매"] = productRows;
    data["판매 없는 상품"] = unsold;
    data["시간대별 매출"] = cur.value("hours");
    data["재고 예측(현재 재고 기준 7일)"] = stock;

    QJsonObject list{{"type", "array"}, {"items", QJsonObject{{"type", "string"}}}};
    QJsonObject schema{
        {"type", "object"},
        {"properties", QJsonObject{{"summary", QJsonObject{{"type", "string"}}},
                                   {"good", list}, {"watch", list}, {"stock", list}, {"actions", list}}},
        {"required", QJsonArray{"summary", "good", "watch", "stock", "actions"}},
        {"additionalProperties", false}};
    QString system =
        "당신은 무인 키오스크 매장의 매출을 관리자에게 보고하는 분석가입니다. "
        "주어진 JSON 의 숫자와 변화율만 근거로 쓰고, 직접 계산하거나 데이터에 없는 내용을 추측하지 마세요. "
        "모든 문장은 한국어 존댓말로 짧게(40자 안팎) 쓰고 숫자를 함께 적으세요. "
        "summary: 이번 기간을 한 문장으로. "
        "good: 잘된 점 1~3개 (늘어난 지표, 잘 팔린 상품). "
        "watch: 살펴볼 점 1~3개 (줄어든 지표, 판매가 줄거나 없는 상품). "
        "stock: 재고 예측에서 3일 안에 떨어지거나 발주가 필요한 상품과 추천 발주 수량 (없으면 빈 배열). "
        "actions: 관리자가 바로 할 수 있는 구체적인 일 3개 (예: 진열 위치 변경, 발주, 바쁜 시간대 대비). "
        "매출이 0이면 판매가 없었다고만 쓰고 원인을 지어내지 마세요.";
    QString answer, aiErr;
    // ---- 오른쪽: AI 분석 (한 줄 요약 + 2x2 칸: 잘된 점 / 살펴볼 점 / 재고·발주 / 할 일) ----
    QString ai;
    if (AI::ask(system, toJson(data), schema, &answer, &aiErr)) {
        QJsonObject a = QJsonDocument::fromJson(answer.toUtf8()).object();
        QString cell = "<td width=50% valign=top style='padding:6px 10px'><h3 style='color:%1'>%2</h3>%3</td>";
        ai += "<table width=100% cellpadding=12 style='background:#cdffcd'><tr><td><b style='font-size:large; color:#14532d'>"
              + a.value("summary").toString().toHtmlEscaped() + "</b></td></tr></table>";
        ai += "<table width=100% cellspacing=0><tr>"
              + cell.arg("#1f7a43", "잘된 점", bullets(a.value("good").toArray()))
              + cell.arg("#c62828", "살펴볼 점", bullets(a.value("watch").toArray())) + "</tr><tr>"
              + cell.arg("#14532d", "재고·발주", bullets(a.value("stock").toArray()))
              + cell.arg("#14532d", "할 일", bullets(a.value("actions").toArray(), true)) + "</tr></table>";
    } else {
        ai += "<h3>AI 분석</h3><p style='color:#c62828'>AI 분석을 받지 못해 숫자만 보여 드립니다.<br>"
              + aiErr.toHtmlEscaped().replace("\n", "<br>") + "</p>";
    }

    // ---- 왼쪽(핵심 지표 아래): 상품별 판매 표 ----
    html += "<h3>상품별 판매</h3>";
    if (productHtml.isEmpty()) {
        html += "<p style='color:#888'>이 기간에 판매된 상품이 없습니다.</p>";
    } else {
        html += "<table width=100% cellspacing=0 cellpadding=6 border=1 style='border-color:#a8dca8'>"
                "<tr style='background:#cdffcd'><th align=left>상품</th><th align=right>수량</th>"
                "<th align=right>매출</th><th align=right>이전</th><th align=right>변화</th></tr>"
                + productHtml + "</table>";
    }
    if (!unsold.isEmpty())
        html += "<p><b>판매 없음:</b> " + unsold.join(", ").toHtmlEscaped() + "</p>";
    if (!peak.isEmpty())
        html += QString("<p><b>가장 바쁜 시간대:</b> %1 (%2)</p>").arg(peak, won(peakAmount));

    QVariantMap res;
    res["kpiHtml"] = html;   // 왼쪽: 숫자 (프로그램이 계산)
    res["aiHtml"] = ai;      // 오른쪽: AI 분석
    res["html"] = html + ai;
    res["title"] = name;
    *out = res;
    return true;
}

// AI_ASK: 질문에 필요한 매출/상품/회원 데이터를 모아 AI 에 묻고 답과 추천 질문 받기
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

// 앱이 보낸 관리자 코드를 server.ini 의 admin/signup_code 와 비교한다.
// 코드가 설정되지 않았으면 관리자 가입은 항상 거부.
bool Server::checkAdminCode(const QString &code, QString *err)
{
    if (m_adminCode.isEmpty()) {
        *err = "이 키오스크는 관리자 회원가입을 받지 않습니다.";
        return false;
    }

    // 길이와 내용이 다른 위치에 상관없이 같은 시간이 걸리도록 끝까지 비교한다
    QByteArray a = code.toUtf8(), b = m_adminCode.toUtf8();
    int diff = a.size() ^ b.size();
    for (int i = 0; i < a.size(); ++i)
        diff |= a.at(i) ^ b.at(i % b.size());
    if (diff == 0)
        return true;

    *err = "관리자 코드가 올바르지 않습니다.";
    return false;
}

static const int MAX_TOKENS = 1000;

// 로그인 토큰 발급 (추측할 수 없는 임의 문자열). 너무 많아지면 오래된 것부터 지움
QString Server::newToken(int userId)
{
    QString token = QUuid::createUuid().toString().remove('{').remove('}').remove('-')
                    + QUuid::createUuid().toString().remove('{').remove('}').remove('-');
    m_tokens.insert(token, userId);
    m_tokenOrder.enqueue(token);
    while (m_tokenOrder.size() > MAX_TOKENS)
        m_tokens.remove(m_tokenOrder.dequeue());
    return token;
}

// 요청의 token 이 지금 관리자 계정의 것인지 (권한은 매번 DB 에서 다시 확인)
bool Server::checkAdmin(const QJsonObject &req, QString *err)
{
    int userId = m_tokens.value(req.value("token").toString(), 0);
    if (!userId) {
        *err = "관리자 로그인이 필요합니다. (로그인 응답의 token 을 함께 보내 주세요)";
        return false;
    }
    QVariant user;
    if (!m_db->user(userId, &user, err))
        return false;
    if (user.toMap().value("role").toString() != "admin") {
        *err = "관리자만 사용할 수 있는 기능입니다.";
        return false;
    }
    return true;
}

// 로그인 결과(회원 정보)에 토큰을 붙임
static void addToken(QVariant *data, const QString &token)
{
    QVariantMap m = data->toMap();
    m["token"] = token;
    *data = m;
}

// 회원가입 사진들을 읽음: 비었거나 사진이 아니면 몇 번째 사진인지 알려 주고 실패
static bool readFaces(const QJsonObject &req, QList<QByteArray> *faces, QString *err)
{
    QJsonArray list = req.value("faces").toArray();
    for (int i = 0; i < list.size(); ++i) {
        QJsonObject one;
        one["image"] = list.at(i);
        QByteArray img;
        if (!readImage(one, &img, err))
            return false;
        QBuffer buf(&img);
        if (img.isEmpty() || !QImageReader(&buf).canRead()) {
            *err = QString("%1번째 얼굴 사진이 비어 있거나 사진 파일(JPEG/PNG)이 아닙니다.").arg(i + 1);
            return false;
        }
        *faces << img;
    }
    return true;
}

static const int TOSS_TIMEOUT = 15000;   // 토스 API 응답을 기다리는 시간(ms)

// 토스 결제 API 에 POST (시크릿 키로 인증). HTTP 상태 코드, 응답이 없으면 0
static int tossPost(const QString &key, const QString &path, const QJsonObject &body, QJsonObject *res)
{
    QNetworkRequest req(QUrl("https://api.tosspayments.com/v1/payments/" + path));
    req.setRawHeader("Authorization", "Basic " + (key + ":").toUtf8().toBase64());
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkAccessManager net;
    QNetworkReply *rep = net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(rep, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(TOSS_TIMEOUT);
    loop.exec();
    if (!rep->isFinished()) {
        rep->abort();
        return 0;
    }
    *res = QJsonDocument::fromJson(rep->readAll()).object();
    return rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

// 토스 응답의 오류 문구 (없으면 상태 코드)
static QString tossError(int status, const QJsonObject &res)
{
    QString msg = res.value("message").toString();
    if (!msg.isEmpty())
        return msg;
    return status ? QString("HTTP %1").arg(status) : QString("토스 응답이 없습니다.");
}

// 토스 결제 최종 승인 (앱이 결제창에서 받은 paymentKey, orderId, amount 를 토스에 확인)
// 승인됐는지 알 수 없으면(응답 없음, 토스 서버 오류, 완료가 아닌 상태) *unsure = true
static bool tossConfirm(const QString &key, const QString &paymentKey, const QString &orderId, int amount,
                        bool *unsure, QString *err)
{
    QJsonObject body{{"paymentKey", paymentKey}, {"orderId", orderId}, {"amount", amount}};
    QJsonObject res;
    int status = tossPost(key, "confirm", body, &res);
    if (status == 200 && res.value("status").toString() == "DONE")
        return true;
    *unsure = status == 0 || status == 200 || status >= 500;
    *err = "결제 승인 실패: " + tossError(status, res);
    return false;
}

// 토스 결제 취소 (승인됐을 수도 있는데 충전하지 못했을 때 돈을 돌려줌)
static bool tossCancel(const QString &key, const QString &paymentKey, QString *err)
{
    QJsonObject res;
    int status = tossPost(key, QUrl::toPercentEncoding(paymentKey) + "/cancel",
                          QJsonObject{{"cancelReason", "잔액 충전 실패"}}, &res);
    if (status == 200)
        return true;
    *err = tossError(status, res);
    return false;
}

// 충전하지 못한 결제를 취소. 그 사이 충전이 됐으면 취소하지 않고, 취소가 안 되면 관리자가 처리하도록 기록
static void refund(DB *db, const QString &key, const QString &paymentKey, const QString &orderId, QString *err)
{
    bool found = false;
    QString dbErr, cancelErr;
    if (db->charged(paymentKey, &found, &dbErr) && found)
        return;
    if (tossCancel(key, paymentKey, &cancelErr)) {
        *err += " (결제는 취소되었습니다)";
        return;
    }
    qWarning("[CHARGE] 결제 취소 실패 paymentKey=%s orderId=%s: %s",
             qPrintable(paymentKey), qPrintable(orderId), qPrintable(cancelErr));
    *err += QString(" 결제가 완료되었다면 관리자에게 문의하세요. (주문번호 %1)").arg(orderId);
}

// 앱 잔액 충전: 비밀번호·금액·중복 확인 → 토스 최종 승인 → 잔액 충전
// 승인은 모든 확인이 끝난 뒤에 하고, 승인 뒤 충전에 실패하면 결제를 취소해서 '돈만 빠지는' 일을 막음
static bool tossCharge(DB *db, const QString &key, const QJsonObject &req, QVariant *out, QString *err)
{
    QString paymentKey = req.value("paymentKey").toString().trimmed();
    QString orderId = req.value("orderId").toString().trimmed();
    int amount = num(req, "amount");
    if (key.isEmpty()) {
        *err = "서버에 토스 시크릿 키가 설정되지 않았습니다. (server.ini 의 [toss] secret_key)";
        return false;
    }
    if (paymentKey.isEmpty() || orderId.isEmpty()) {
        *err = "결제 정보(paymentKey, orderId)가 없습니다.";
        return false;
    }
    QVariant who;
    bool found = false;
    if (!db->login(req.value("loginId").toString(), req.value("password").toString(), &who, err)
        || !DB::checkCharge(amount, err) || !db->charged(paymentKey, &found, err))
        return false;
    if (found) {
        *err = "이미 충전된 결제입니다.";
        return false;
    }

    bool unsure = false;
    if (!tossConfirm(key, paymentKey, orderId, amount, &unsure, err)) {
        if (unsure)
            refund(db, key, paymentKey, orderId, err);
        return false;
    }
    if (db->topUp(who.toMap().value("id").toInt(), amount, paymentKey, orderId, out, err))
        return true;
    refund(db, key, paymentKey, orderId, err);
    return false;
}

// 관리자만 쓸 수 있는 명령 (상품 추가/수정/삭제, 매출, 회원 목록, AI)
static bool adminOnly(const QString &cmd)
{
    static const QSet<QString> cmds{"ADD_PRODUCT", "UPDATE_PRODUCT", "DELETE_PRODUCT", "SALES",
                                    "MEMBERS", "AI_REPORT", "AI_ASK", "AI_FORECAST"};
    return cmds.contains(cmd);
}

// 키오스크 화면(같은 PC)만 쓸 수 있는 명령 (얼굴 로그인, 얼굴 사진 목록, 결제)
// 본인 확인 없이 회원 번호만으로 처리하므로 다른 PC 에서 오면 거절
static bool kioskOnly(const QString &cmd)
{
    static const QSet<QString> cmds{"LOGIN", "FACES", "PAY"};
    return cmds.contains(cmd);
}

// 요청 하나 처리: cmd 에 따라 DB/AI 함수 호출 후 {ok, data | error} 응답 생성
QJsonObject Server::handle(const QJsonObject &req, bool local)
{
    QString cmd = req.value("cmd").toString();
    QVariant data;
    QString err;
    bool ok = false;

    if (kioskOnly(cmd) && !local) {
        err = "키오스크에서만 사용할 수 있는 기능입니다.";
    } else if (adminOnly(cmd) && !checkAdmin(req, &err)) {
        // 권한 없음: 아래 처리를 건너뛰고 오류 응답
    } else if (cmd == "LOGIN") {
        // 얼굴인식 로그인 (키오스크 화면에서만 옴)
        ok = m_db->user(num(req, "userId"), &data, &err);
        if (ok)
            addToken(&data, newToken(num(req, "userId")));
    } else if (cmd == "PASSWORD_LOGIN") {
        ok = m_db->login(req.value("loginId").toString(), req.value("password").toString(), &data, &err);
        if (ok)
            addToken(&data, newToken(data.toMap().value("id").toInt()));
    } else if (cmd == "LOGOUT") {
        m_tokens.remove(req.value("token").toString());
        ok = true;
    } else if (cmd == "REGISTER") {
        // 관리자 코드 외에는 모두 필수: 빈 칸이 있으면 어느 칸인지 알려 주고 거절
        struct Field { const char *key, *label; };
        const Field fields[] = {{"name", "이름"}, {"loginId", "아이디"}, {"password", "비밀번호"},
                                {"phone", "전화번호"}};
        ok = true;
        for (const Field &f : fields) {
            if (req.value(f.key).toString().trimmed().isEmpty()) {
                err = QString("%1(%2)을(를) 입력하세요.").arg(f.label, f.key);
                ok = false;
                break;
            }
        }
        QList<QByteArray> faces;
        ok = ok && readFaces(req, &faces, &err);
        QJsonValue code = req.value("adminCode");
        QString adminCode = (code.isDouble() ? QString::number(code.toVariant().toLongLong())
                                             : code.toString()).trimmed();
        QString role = "member";
        if (ok && !adminCode.isEmpty()) {
            ok = checkAdminCode(adminCode, &err);
            role = "admin";
        }
        ok = ok && m_db->join(req.value("name").toString(), req.value("loginId").toString(),
                              req.value("password").toString(), req.value("phone").toString(),
                              role, faces, &data, &err);
    } else if (cmd == "CHECK_ID") {
        QString id = req.value("loginId").toString().trimmed();
        bool available = false;
        if (id.isEmpty())
            err = "아이디(loginId)를 입력하세요.";
        else
            ok = m_db->idAvailable(id, &available, &err);
        if (ok)
            data = QVariantMap{{"loginId", id}, {"available", available},
                               {"message", available ? "사용할 수 있는 아이디입니다." : "이미 사용 중인 아이디입니다."}};
    } else if (cmd == "CHANGE_ACCOUNT") {
        ok = m_db->editAccount(req.value("loginId").toString(), req.value("password").toString(),
                               req.value("newLoginId").toString(), req.value("newPassword").toString(),
                               &data, &err);
    } else if (cmd == "CHARGE") {
        ok = tossCharge(m_db, m_tossKey, req, &data, &err);
    } else if (cmd == "HISTORY") {
        ok = m_db->history(num(req, "userId"), req.value("from").toString(),
                           req.value("to").toString(), &data, &err);
    } else if (cmd == "FACES") {
        ok = m_db->faces(num(req, "afterId"), &data, &err);
    } else if (cmd == "PRODUCTS") {
        ok = m_db->products(&data, &err);
    } else if (cmd == "ADD_PRODUCT") {
        QByteArray img;
        ok = readImage(req, &img, &err)
             && m_db->addProduct(req.value("name").toString(), num(req, "price"),
                                 num(req, "stock"), num(req, "shelf"), img, &err);
    } else if (cmd == "UPDATE_PRODUCT") {
        QVariantMap f;
        if (has(req, "name"))
            f["name"] = req.value("name").toString();
        if (has(req, "price"))
            f["price"] = num(req, "price");
        if (has(req, "stock"))
            f["stock"] = num(req, "stock");
        if (has(req, "shelf"))
            f["shelf"] = num(req, "shelf");
        QByteArray img;
        ok = readImage(req, &img, &err);
        if (ok) {
            if (!img.isEmpty())
                f["image"] = img;
            ok = m_db->editProduct(num(req, "productId"), f, &err);
        }
    } else if (cmd == "DELETE_PRODUCT") {
        ok = m_db->delProduct(num(req, "productId"), &err);
    } else if (cmd == "PAY") {
        ok = m_db->pay(num(req, "userId"), req.value("method").toString(),
                       req.value("cardUid").toString(), req.value("items").toArray().toVariantList(),
                       &data, &err);
    } else if (cmd == "SALES") {
        ok = m_db->sales(req.value("unit").toString(), req.value("date").toString(), &data, &err);
    } else if (cmd == "MEMBERS") {
        ok = m_db->members(&data, &err);
    } else if (cmd == "AI_REPORT") {
        ok = aiReport(m_db, req.value("unit").toString(), req.value("date").toString(), &data, &err);
    } else if (cmd == "AI_ASK") {
        ok = aiAsk(m_db, req.value("question").toString(), num(req, "productId"),
                   num(req, "userId"), &data, &err);
    } else if (cmd == "AI_FORECAST") {
        ok = aiForecast(m_db, num(req, "productId"), num(req, "days"), &data, &err);
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

// server.ini 찾기 (현재 폴더 → 실행 파일 폴더 → server 소스 폴더 순서)
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

// 프로그램 종료 시 서버 스레드 정리
ServerThread::~ServerThread()
{
    quit();
    wait();
}

// 서버 스레드를 시작하고 준비(성공/실패)될 때까지 기다림
bool ServerThread::startWait(QString *err)
{
    start();
    m_ready.acquire();
    *err = m_err;
    return m_ok;
}

// 서버 스레드 본체: server.ini 읽기, AI 설정, DB 연결, 9000번 포트 열기
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
    server.setAdminCode(ini.value("admin/signup_code").toString().trimmed());
    server.setTossKey(ini.value("toss/secret_key").toString().trimmed());
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

// ESP32 연결 서버 생성, 진열대는 모두 '물건 있음'으로 시작
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

// 프로그램에 하나뿐인 Esp 객체 (없으면 0)
Esp *Esp::get()
{
    return instance;
}

// 그 진열대가 비었는지 (0 이나 범위 밖이면 false)
bool Esp::shelfEmpty(int shelf)
{
    if (!instance || shelf < 1 || shelf > instance->m_shelf.size())
        return false;
    return instance->m_shelf.at(shelf - 1).toInt() == 0;
}

// 8000번 포트 열기
bool Esp::start(QString *err)
{
    if (listen(QHostAddress::Any, ESP_PORT))
        return true;
    *err = QString("ESP32 포트 %1 열기 실패: %2").arg(ESP_PORT).arg(errorString());
    return false;
}

// ESP32 가 지금 연결되어 있는지
bool Esp::connected() const
{
    return m_sock && m_sock->state() == QAbstractSocket::ConnectedState;
}

// ESP32 접속 (새로 접속하면 이전 연결은 끊음)
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

// 받은 데이터를 줄 단위로 나눠 handle() 로 처리
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

// ESP32 연결이 끊김
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

// ESP32 명령 한 줄 처리: person(사람 감지), card(카드 UID), stand N(진열대 상태)
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

// ESP32 에 명령 한 줄 보내기 (연결 안 됐으면 로그만)
void Esp::send(const QString &cmd)
{
    if (!connected()) {
        qDebug("[ESP32] 연결 안 됨, 보내지 못함: %s", qPrintable(cmd));
        return;
    }
    m_sock->write(cmd.toUtf8() + '\n');
}

// 결제 성공 효과음
void Esp::buzzer()
{
    send("BUZZER");
}

// 카드 리더기 켜기
void Esp::cardSelect()
{
    send("CARD_SELECT");
}

// 카드 결제 실패 알림 (리더기는 켜 둔 채 다른 카드 대기)
void Esp::cardFail()
{
    send("CARD_FAIL");
}

// 카드 리더기 끄기
void Esp::cardCancel()
{
    send("CARD_CANCEL");
}
