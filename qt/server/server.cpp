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
#include <QSet>
#include <QSettings>
#include <QStringList>
#include <QTcpSocket>

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

// 변화율 칸 HTML (증가는 초록, 감소는 빨강)
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
//  - 숫자(핵심 지표, 상품별 판매, 재고)는 서버가 계산해서 표로 보여 주고
//  - AI(Groq)는 그 숫자를 근거로 한 줄 요약, 잘된 점, 살펴볼 점, 재고·발주, 할 일만 작성
//  - AI 가 실패해도 숫자 표는 그대로 보여 줌
//  - 가로 화면용으로 kpiHtml(왼쪽 숫자) / aiHtml(오른쪽 AI 분석)을 나눠서 돌려줌
static bool aiReport(DB *db, const QString &unit, const QString &date, QVariant *out, QString *err)
{
    QDate d = QDate::fromString(date, "yyyy-MM-dd");
    if (!d.isValid())
        d = QDate::currentDate();

    // 기간: 일별은 그날, 주별은 그 주 월~일, 월별은 그 달. 이전 기간은 바로 앞 같은 단위
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
    // 진행 중인 기간(오늘, 이번 주, 이번 달)은 지금까지만, 이전 기간도 같은 경과 시간만큼만 비교
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

    // ---- 핵심 지표 표 ----
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

    // ---- 상품별 비교 (AI 에도 같은 숫자를 넘김) ----
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

    // 매출이 가장 많은 시간대
    QString peak;
    int peakAmount = 0;
    foreach (const QVariant &v, cur.value("hours").toList()) {
        QVariantMap m = v.toMap();
        if (m.value("amount").toInt() > peakAmount) {
            peakAmount = m.value("amount").toInt();
            peak = QString("%1시").arg(m.value("hour").toInt());
        }
    }

    // ---- AI 분석 ----
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

// 요청 하나 처리: cmd 에 따라 DB/AI 함수 호출 후 {ok, data | error} 응답 생성
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
        // adminCode 가 없거나 비어 있으면 일반 회원, 있으면 맞아야만 관리자로 가입
        QString adminCode = req.value("adminCode").toString();
        QString role = "member";
        if (ok && !adminCode.isEmpty()) {
            ok = checkAdminCode(adminCode, &err);
            role = "admin";
        }
        ok = ok && m_db->join(req.value("name").toString(), req.value("loginId").toString(),
                              req.value("password").toString(), role, faces, &data, &err);
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
        ok = aiReport(m_db, req.value("unit").toString(), req.value("date").toString(), &data, &err);
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
