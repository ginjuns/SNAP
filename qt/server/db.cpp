#include "db.h"
#include "packet.h"

#include <QtSql>
#include <QDateTime>
#include <QSettings>

// 쿼리 오류 메시지를 err 에 넣고 false
static bool fail(const QSqlQuery &q, QString *err)
{
    *err = q.lastError().text();
    return false;
}

// 트랜잭션을 되돌리고 err 에 메시지를 넣은 뒤 false
static bool rollback(const QString &msg, QString *err)
{
    QSqlDatabase::database().rollback();
    *err = msg;
    return false;
}

// 같은 이름의 상품이 있는지 (except 번 상품은 제외, 수정할 때 사용)
static bool exists(const QString &name, int except, bool *found, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM products WHERE name = ? AND id <> ?");
    q.addBindValue(name);
    q.addBindValue(except);
    if (!q.exec() || !q.next())
        return fail(q, err);
    *found = q.value(0).toInt() > 0;
    return true;
}

static const int MIN_FACES = 5;           // 회원가입에 필요한 얼굴 사진 수
static const int MAX_CHARGE = 1000000;    // 한 번에 충전할 수 있는 최대 금액

// 아이디 중복(UNIQUE) 위반이면 알맞은 문구로 바꿔서 err 에 넣음
static QString dupMessage(const QSqlError &e)
{
    if (e.nativeErrorCode() != "1062")
        return e.text();
    if (e.text().contains("phone"))
        return "이미 가입된 전화번호입니다.";
    return "이미 사용 중인 아이디입니다.";
}

// 전화번호를 숫자만 남겨 정리 (010-1234-5678 -> 01012345678), 형식이 틀리면 빈 문자열
static QString normPhone(const QString &phone)
{
    QString digits;
    foreach (QChar c, phone) {
        if (c.isDigit())
            digits += c;
        else if (c != '-' && c != ' ')
            return QString();
    }
    if (digits.size() < 10 || digits.size() > 11 || !digits.startsWith('0'))
        return QString();
    return digits;
}

// 아이디 형식 검사 (1~30자, 공백 없음)
static bool checkId(const QString &id, QString *err)
{
    bool space = false;
    foreach (QChar c, id)
        space = space || c.isSpace();
    if (id.isEmpty() || id.size() > 30 || space) {
        *err = "아이디는 공백 없이 1~30자로 입력하세요.";
        return false;
    }
    return true;
}

// 비밀번호 형식 검사 (1~100자)
static bool checkPw(const QString &pw, QString *err)
{
    if (pw.trimmed().isEmpty() || pw.size() > 100) {
        *err = "비밀번호는 1~100자로 입력하세요. (공백만으로는 안 됨)";
        return false;
    }
    return true;
}

// 같은 값을 가진 회원이 있는지 (column: login_id / phone, except 번 회원은 제외)
static bool taken(const QString &column, const QString &value, int except, bool *found, QString *err)
{
    QSqlQuery q;
    q.prepare(QString("SELECT COUNT(*) FROM users WHERE %1 = ? AND id <> ?").arg(column));
    q.addBindValue(value);
    q.addBindValue(except);
    if (!q.exec() || !q.next())
        return fail(q, err);
    *found = q.value(0).toInt() > 0;
    return true;
}

// 아이디/비밀번호가 맞으면 회원 번호를 userId 에
static bool auth(const QString &id, const QString &pw, int *userId, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT id FROM users WHERE login_id = ? AND password = ?");
    q.addBindValue(id.trimmed());
    q.addBindValue(pw);
    if (!q.exec())
        return fail(q, err);
    if (!q.next()) {
        *err = "아이디 또는 비밀번호가 올바르지 않습니다.";
        return false;
    }
    *userId = q.value(0).toInt();
    return true;
}

// 매출 기록의 상품명. 상품이 삭제되면 product_id 가 NULL 이 되므로 앞에 "(삭제) " 를 붙임
// (같은 이름으로 새로 등록한 상품과도 구분됨)
#define SOLD_NAME(t) "CASE WHEN " t "product_id IS NULL THEN CONCAT('(삭제) ', " t "product_name)" \
                     " ELSE " t "product_name END"

// 진열대 번호 검사, 0(선택 안 함)이면 DB 에 NULL 로 저장
static bool shelfValue(int shelf, QVariant *v, QString *err)
{
    if (shelf < 0 || shelf > SHELF_COUNT) {
        *err = QString("진열대 번호는 1~%1 이어야 합니다.").arg(SHELF_COUNT);
        return false;
    }
    *v = shelf ? QVariant(shelf) : QVariant(QVariant::Int);
    return true;
}

// server.ini 의 [mysql] 로 DB 접속, 필요한 테이블/칸이 있는지 확인
bool DB::open(const QSettings &ini, QString *err)
{
    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        *err = "Qt MySQL 드라이버(QMYSQL)가 없습니다.\n"
               "Ubuntu: sudo apt install libqt5sql5-mysql";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(ini.value("mysql/host", "127.0.0.1").toString());
    db.setPort(ini.value("mysql/port", 3306).toInt());
    db.setDatabaseName(ini.value("mysql/database", "kiosk").toString());
    db.setUserName(ini.value("mysql/user", "kiosk").toString());
    db.setPassword(ini.value("mysql/password").toString());
    db.setConnectOptions("MYSQL_OPT_RECONNECT=1");
    if (!db.open()) {
        *err = db.lastError().text();
        return false;
    }

    const char *tables[] = { "users", "products", "sales" };
    QSqlQuery q;
    for (int i = 0; i < 3; ++i) {
        if (!q.exec(QString("SELECT 1 FROM %1 LIMIT 1").arg(tables[i]))) {
            *err = QString("'%1' 테이블을 읽을 수 없습니다. sudo mysql < db/kiosk.sql 을 먼저 실행하세요.\n%2")
                       .arg(tables[i]).arg(q.lastError().text());
            return false;
        }
    }
    if (!q.exec("SELECT stock FROM products LIMIT 1")) {
        *err = "products 테이블에 재고(stock) 칸이 없습니다.\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    if (!q.exec("SELECT shelf FROM products LIMIT 1")) {
        *err = "products 테이블에 진열대(shelf) 칸이 없습니다.\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    if (!q.exec("SELECT login_id FROM users LIMIT 1")) {
        *err = "users 테이블에 비밀번호 로그인(login_id, password) 칸이 없습니다.\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    if (!q.exec("SELECT password FROM users LIMIT 1")) {
        *err = "비밀번호 저장 방식이 바뀌었습니다 (암호화 -> 글자 그대로).\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    if (!q.exec("SELECT phone FROM users LIMIT 1")) {
        *err = "users 테이블에 전화번호(phone) 칸이 없습니다.\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    if (!q.exec("SELECT 1 FROM face_images LIMIT 1")) {
        *err = "얼굴 사진(face_images) 테이블이 없습니다.\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    if (!q.exec("SELECT 1 FROM cards LIMIT 1")) {
        *err = "카드(cards) 테이블이 없습니다.\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    if (!q.exec("SELECT payment_key FROM charges LIMIT 1")) {
        *err = "충전 기록(charges) 테이블이 없거나 결제 키(payment_key) 칸이 없습니다.\n"
               "DB를 최신으로 맞추려면 sudo mysql < db/kiosk.sql 을 실행하세요.";
        return false;
    }
    return true;
}

// 회원가입: 아이디/전화번호 중복 확인 후 회원과 얼굴 사진을 한 트랜잭션으로 저장
bool DB::join(const QString &name0, const QString &id0, const QString &pw, const QString &phone0,
              const QString &role, const QList<QByteArray> &faces, QVariant *out, QString *err)
{
    QString name = name0.trimmed();
    QString id = id0.trimmed();
    if (name.isEmpty() || id.isEmpty() || pw.trimmed().isEmpty() || phone0.trimmed().isEmpty()) {
        *err = "이름, 아이디, 비밀번호, 전화번호를 모두 입력하세요.";
        return false;
    }
    if (name.size() > 50) {
        *err = "이름은 50자 이내로 입력하세요.";
        return false;
    }
    if (!checkId(id, err) || !checkPw(pw, err))
        return false;
    QString phone = normPhone(phone0);
    if (phone.isEmpty()) {
        *err = "전화번호 형식이 올바르지 않습니다. (예: 010-1234-5678)";
        return false;
    }
    if (faces.size() < MIN_FACES) {
        *err = QString("얼굴 사진 %1장이 필요합니다. (받은 사진 %2장)").arg(MIN_FACES).arg(faces.size());
        return false;
    }

    bool found = false;
    if (!taken("login_id", id, 0, &found, err))
        return false;
    if (found) {
        *err = QString("'%1' 아이디는 이미 사용 중입니다.").arg(id);
        return false;
    }
    if (!taken("phone", phone, 0, &found, err))
        return false;
    if (found) {
        *err = "이미 가입된 전화번호입니다.";
        return false;
    }

    QSqlQuery q;
    QSqlDatabase::database().transaction();
    q.prepare("INSERT INTO users (name, role, login_id, password, phone) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(name);
    q.addBindValue(role);
    q.addBindValue(id);
    q.addBindValue(pw);
    q.addBindValue(phone);
    if (!q.exec())
        return rollback(dupMessage(q.lastError()), err);
    int userId = q.lastInsertId().toInt();

    foreach (const QByteArray &img, faces) {
        q.prepare("INSERT INTO face_images (user_id, image) VALUES (?, ?)");
        q.addBindValue(userId);
        q.addBindValue(img);
        if (!q.exec())
            return rollback(q.lastError().text(), err);
    }

    if (!QSqlDatabase::database().commit())
        return rollback(QSqlDatabase::database().lastError().text(), err);
    QVariantMap m;
    m["userId"] = userId;
    m["role"] = role;
    *out = m;
    return true;
}

// after 번 이후에 등록된 얼굴 사진 목록 (키오스크가 새로 가입한 얼굴만 받아 감)
bool DB::faces(int after, QVariant *out, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT id, user_id, image FROM face_images WHERE id > ? ORDER BY id");
    q.addBindValue(after);
    if (!q.exec())
        return fail(q, err);
    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0).toInt();
        m["userId"] = q.value(1).toInt();
        m["image"] = QString::fromLatin1(q.value(2).toByteArray().toBase64());
        list << m;
    }
    *out = list;
    return true;
}

// 아이디/비밀번호 확인, 맞으면 회원 id
bool DB::login(const QString &id, const QString &pw, QVariant *out, QString *err)
{
    int userId = 0;
    return auth(id, pw, &userId, err) && user(userId, out, err);
}

// 앱 회원가입 화면의 중복 확인: 아이디/전화번호 중 보낸 것만 검사
bool DB::checkDup(const QString &id0, const QString &phone0, QVariant *out, QString *err)
{
    QString id = id0.trimmed();
    if (id.isEmpty() && phone0.trimmed().isEmpty()) {
        *err = "확인할 아이디(loginId) 또는 전화번호(phone)를 보내 주세요.";
        return false;
    }
    QVariantMap m;
    bool found = false;
    if (!id.isEmpty()) {
        if (!checkId(id, err) || !taken("login_id", id, 0, &found, err))
            return false;
        m["loginIdAvailable"] = !found;
    }
    if (!phone0.trimmed().isEmpty()) {
        QString phone = normPhone(phone0);
        if (phone.isEmpty()) {
            *err = "전화번호 형식이 올바르지 않습니다. (예: 010-1234-5678)";
            return false;
        }
        if (!taken("phone", phone, 0, &found, err))
            return false;
        m["phoneAvailable"] = !found;
    }
    *out = m;
    return true;
}

// 회원 아이디/비밀번호 변경: 지금 아이디·비밀번호가 맞아야 하고, 보낸 항목만 바꿈
bool DB::editAccount(const QString &id, const QString &pw, const QString &newId0, const QString &newPw,
                     QVariant *out, QString *err)
{
    int userId = 0;
    if (!auth(id, pw, &userId, err))
        return false;

    QString newId = newId0.trimmed();
    QStringList sets;
    QVariantList vals;
    if (!newId.isEmpty()) {
        bool found = false;
        if (!checkId(newId, err) || !taken("login_id", newId, userId, &found, err))
            return false;
        if (found) {
            *err = QString("'%1' 아이디는 이미 사용 중입니다.").arg(newId);
            return false;
        }
        sets << "login_id = ?";
        vals << newId;
    }
    if (!newPw.isEmpty()) {
        if (!checkPw(newPw, err))
            return false;
        sets << "password = ?";
        vals << newPw;
    }
    if (sets.isEmpty()) {
        *err = "바꿀 아이디(newLoginId) 또는 비밀번호(newPassword)를 입력하세요.";
        return false;
    }

    QSqlQuery q;
    q.prepare("UPDATE users SET " + sets.join(", ") + " WHERE id = ?");
    foreach (const QVariant &v, vals)
        q.addBindValue(v);
    q.addBindValue(userId);
    if (!q.exec()) {
        *err = dupMessage(q.lastError());
        return false;
    }
    return user(userId, out, err);
}

// 충전 금액 검사 (토스 승인 전에 확인해서, 승인 뒤 충전이 거절되는 일을 막음)
bool DB::checkCharge(int amount, QString *err)
{
    if (amount <= 0 || amount > MAX_CHARGE) {
        *err = QString("충전 금액은 1원 ~ %L1원 사이로 입력하세요.").arg(MAX_CHARGE);
        return false;
    }
    return true;
}

// 이 토스 결제(paymentKey)로 이미 충전했는지
bool DB::charged(const QString &paymentKey, bool *found, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM charges WHERE payment_key = ?");
    q.addBindValue(paymentKey);
    if (!q.exec() || !q.next())
        return fail(q, err);
    *found = q.value(0).toInt() > 0;
    return true;
}

// 잔액 충전: 토스 승인이 끝난 결제의 금액을 더하고 충전 기록을 남김
bool DB::topUp(int userId, int amount, const QString &paymentKey, const QString &orderId,
               QVariant *out, QString *err)
{
    if (!checkCharge(amount, err))
        return false;

    // 잔액 증가와 충전 기록 저장은 함께 성공하거나 함께 취소
    QSqlQuery q;
    QSqlDatabase::database().transaction();
    q.prepare("UPDATE users SET balance = balance + ? WHERE id = ?");
    q.addBindValue(amount);
    q.addBindValue(userId);
    if (!q.exec())
        return rollback(q.lastError().text(), err);
    if (q.numRowsAffected() == 0)
        return rollback("등록되지 않은 사용자입니다.", err);
    q.prepare("INSERT INTO charges (user_id, amount, payment_key, order_id) VALUES (?, ?, ?, ?)");
    q.addBindValue(userId);
    q.addBindValue(amount);
    q.addBindValue(paymentKey);
    q.addBindValue(orderId);
    if (!q.exec())
        return rollback(q.lastError().nativeErrorCode() == "1062" ? "이미 충전된 결제입니다."
                                                                  : q.lastError().text(), err);
    if (!QSqlDatabase::database().commit())
        return rollback(QSqlDatabase::database().lastError().text(), err);
    return user(userId, out, err);
}

// 이용 내역: from ~ to(미포함) 사이의 결제(use, 결제 1건 = 1줄)와 충전(charge)을 시간순으로
bool DB::history(int userId, const QString &from0, const QString &to0, QVariant *out, QString *err)
{
    QDate fromDay = QDate::fromString(from0, "yyyy-MM-dd");
    QDate toDay = QDate::fromString(to0, "yyyy-MM-dd");
    if (!fromDay.isValid() || !toDay.isValid()) {
        *err = "날짜 형식이 잘못되었습니다. (from, to: yyyy-MM-dd)";
        return false;
    }
    QDateTime from(fromDay);
    QDateTime to(toDay);

    // 결제 한 번의 상품 줄들은 sold_at 이 같으므로 sold_at 으로 묶어 합계
    QSqlQuery q;
    q.prepare("SELECT sold_at, 'use', SUM(amount) FROM sales"
              " WHERE user_id = ? AND sold_at >= ? AND sold_at < ? GROUP BY sold_at"
              " UNION ALL"
              " SELECT charged_at, 'charge', amount FROM charges"
              " WHERE user_id = ? AND charged_at >= ? AND charged_at < ?"
              " ORDER BY 1");
    for (int i = 0; i < 2; ++i) {
        q.addBindValue(userId);
        q.addBindValue(from);
        q.addBindValue(to);
    }
    if (!q.exec())
        return fail(q, err);
    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["time"] = q.value(0).toDateTime().toString("yyyy-MM-dd HH:mm:ss");
        m["type"] = q.value(1).toString();
        m["amount"] = q.value(2).toInt();
        list << m;
    }
    *out = list;
    return true;
}

// 회원 정보 (이름, 권한, 잔액, 아이디, 전화번호)
bool DB::user(int id, QVariant *out, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT id, name, role, balance, login_id, phone FROM users WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec())
        return fail(q, err);
    if (!q.next()) {
        *err = "등록되지 않은 사용자입니다.";
        return false;
    }
    QVariantMap m;
    m["id"] = q.value(0).toInt();
    m["name"] = q.value(1).toString();
    m["role"] = q.value(2).toString();
    m["balance"] = q.value(3).toInt();
    m["loginId"] = q.value(4).toString();
    m["phone"] = q.value(5).toString();
    *out = m;
    return true;
}

// 상품 목록 (사진은 Base64)
bool DB::products(QVariant *out, QString *err)
{
    QSqlQuery q;
    if (!q.exec("SELECT id, name, price, stock, shelf, image FROM products ORDER BY id"))
        return fail(q, err);
    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0).toInt();
        m["name"] = q.value(1).toString();
        m["price"] = q.value(2).toInt();
        m["stock"] = q.value(3).toInt();
        m["shelf"] = q.value(4).toInt();   // NULL -> 0 (지정 안 함)
        m["image"] = QString::fromLatin1(q.value(5).toByteArray().toBase64());
        list << m;
    }
    *out = list;
    return true;
}

// 상품 추가 (같은 이름 금지)
bool DB::addProduct(const QString &name0, int price, int stock, int shelf, const QByteArray &img, QString *err)
{
    QString name = name0.trimmed();
    if (name.isEmpty() || price <= 0 || stock < 0) {
        *err = "상품명, 가격, 재고를 올바르게 입력하세요.";
        return false;
    }
    QVariant shelfVal;
    if (!shelfValue(shelf, &shelfVal, err))
        return false;
    bool found = false;
    if (!exists(name, 0, &found, err))
        return false;
    if (found) {
        *err = QString("'%1' 상품이 이미 등록되어 있습니다.\n다른 상품명을 입력하세요.").arg(name);
        return false;
    }

    QSqlQuery q;
    q.prepare("INSERT INTO products (name, price, stock, shelf, image) VALUES (?, ?, ?, ?, ?)");
    q.addBindValue(name);
    q.addBindValue(price);
    q.addBindValue(stock);
    q.addBindValue(shelfVal);
    q.addBindValue(img.isEmpty() ? QVariant(QVariant::ByteArray) : QVariant(img));
    return q.exec() || fail(q, err);
}

// 상품 수정, f 에 들어 있는 항목만 바꿈
bool DB::editProduct(int id, const QVariantMap &f, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM products WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return fail(q, err);
    if (q.value(0).toInt() == 0) {
        *err = "없는 상품입니다.";
        return false;
    }

    QStringList sets;
    QVariantList vals;
    if (f.contains("name")) {
        QString name = f.value("name").toString().trimmed();
        bool found = false;
        if (name.isEmpty()) {
            *err = "상품명을 입력하세요.";
            return false;
        }
        if (!exists(name, id, &found, err))
            return false;
        if (found) {
            *err = QString("'%1' 상품이 이미 등록되어 있습니다.").arg(name);
            return false;
        }
        sets << "name = ?";
        vals << name;
    }
    if (f.contains("price")) {
        if (f.value("price").toInt() <= 0) {
            *err = "가격을 올바르게 입력하세요.";
            return false;
        }
        sets << "price = ?";
        vals << f.value("price").toInt();
    }
    if (f.contains("stock")) {
        if (f.value("stock").toInt() < 0) {
            *err = "재고는 0 이상이어야 합니다.";
            return false;
        }
        sets << "stock = ?";
        vals << f.value("stock").toInt();
    }
    if (f.contains("shelf")) {
        QVariant shelfVal;
        if (!shelfValue(f.value("shelf").toInt(), &shelfVal, err))
            return false;
        sets << "shelf = ?";
        vals << shelfVal;
    }
    if (f.contains("image")) {
        QByteArray img = f.value("image").toByteArray();
        sets << "image = ?";
        vals << (img.isEmpty() ? QVariant(QVariant::ByteArray) : QVariant(img));
    }
    if (sets.isEmpty()) {
        *err = "변경할 항목(name, price, stock, shelf, image)이 없습니다.";
        return false;
    }

    q.prepare("UPDATE products SET " + sets.join(", ") + " WHERE id = ?");
    foreach (const QVariant &v, vals)
        q.addBindValue(v);
    q.addBindValue(id);
    return q.exec() || fail(q, err);
}

// 상품 삭제 (매출 기록의 상품은 NULL 로 남음)
bool DB::delProduct(int id, QString *err)
{
    QSqlQuery q;
    q.prepare("DELETE FROM products WHERE id = ?");
    q.addBindValue(id);
    if (!q.exec())
        return fail(q, err);
    if (q.numRowsAffected() == 0) {
        *err = "이미 삭제되었거나 없는 상품입니다.";
        return false;
    }
    return true;
}

// 잔액 차감 (회원 users 또는 카드 cards), 잔액이 부족하면 되돌리고 오류
static bool charge(const QString &table, const QString &key, const QVariant &id, int total,
                   QVariantMap *res, QString *err)
{
    bool card = table == "cards";
    QSqlQuery q;
    q.prepare(QString("UPDATE %1 SET balance = balance - ? WHERE %2 = ? AND balance >= ?").arg(table, key));
    q.addBindValue(total);
    q.addBindValue(id);
    q.addBindValue(total);
    if (!q.exec())
        return rollback(q.lastError().text(), err);
    bool paid = q.numRowsAffected() > 0;

    q.prepare(QString("SELECT balance FROM %1 WHERE %2 = ?").arg(table, key));
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return rollback(card ? "등록되지 않은 카드입니다." : "등록되지 않은 사용자입니다.", err);
    int balance = q.value(0).toInt();
    if (!paid)
        return rollback(QString("%1잔액이 부족합니다. (잔액 %2원 / 결제금액 %3원)")
                            .arg(card ? "카드 " : "").arg(balance).arg(total), err);
    (*res)["balance"] = balance;
    return true;
}

// 결제: 재고 확인·차감, 회원/카드 잔액 차감, 매출 기록을 한 트랜잭션으로 처리
bool DB::pay(int userId, const QString &method, const QString &cardUid, const QVariantList &items,
             QVariant *out, QString *err)
{
    if (items.isEmpty()) {
        *err = "장바구니가 비어 있습니다.";
        return false;
    }
    if (method != "card" && method != "face") {
        *err = "알 수 없는 결제 수단입니다.";
        return false;
    }

    QSqlDatabase::database().transaction();
    QSqlQuery q;
    QDateTime now = QDateTime::currentDateTime();
    int total = 0;

    foreach (const QVariant &v, items) {
        QVariantMap it = v.toMap();
        int pid = it.value("productId").toInt();
        int qty = it.value("qty").toInt();
        q.prepare("SELECT price, name, stock FROM products WHERE id = ?");
        q.addBindValue(pid);
        if (qty <= 0 || !q.exec() || !q.next())
            return rollback("잘못된 상품 정보입니다. (삭제된 상품일 수 있습니다)", err);

        int amount = q.value(0).toInt() * qty;
        QString name = q.value(1).toString();
        int stock = q.value(2).toInt();
        total += amount;

        q.prepare("UPDATE products SET stock = stock - ? WHERE id = ? AND stock >= ?");
        q.addBindValue(qty);
        q.addBindValue(pid);
        q.addBindValue(qty);
        if (!q.exec())
            return rollback(q.lastError().text(), err);
        if (q.numRowsAffected() == 0)
            return rollback(QString("'%1' 재고가 부족합니다. (남은 수량 %2개)").arg(name).arg(stock), err);

        q.prepare("INSERT INTO sales (user_id, product_id, product_name, qty, amount, method, sold_at)"
                  " VALUES (?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(userId);
        q.addBindValue(pid);
        q.addBindValue(name);
        q.addBindValue(qty);
        q.addBindValue(amount);
        q.addBindValue(method);
        q.addBindValue(now);
        if (!q.exec())
            return rollback(q.lastError().text(), err);
    }

    QVariantMap res;
    res["total"] = total;

    bool charged = method == "face" ? charge("users", "id", userId, total, &res, err)
                                    : charge("cards", "uid", cardUid, total, &res, err);
    if (!charged)
        return false;

    if (!QSqlDatabase::database().commit())
        return rollback(QSqlDatabase::database().lastError().text(), err);
    *out = res;
    return true;
}

// 매출 조회: 시간대(일별)/날짜(월별)별 카드·얼굴인식 합계와 상세 내역
bool DB::sales(const QString &unit, const QString &date, QVariant *out, QString *err)
{
    bool mon = (unit == "month");
    QDate day = QDate::fromString(mon ? date + "-01" : date, "yyyy-MM-dd");
    if (!day.isValid()) {
        *err = "날짜 형식이 잘못되었습니다. (일별: yyyy-MM-dd, 월별: yyyy-MM)";
        return false;
    }
    QDateTime from(day);
    QDateTime to(mon ? day.addMonths(1) : day.addDays(1));

    QSqlQuery q;
    q.prepare(QString("SELECT %1 AS bucket,"
                      " SUM(CASE WHEN method = 'card' THEN amount ELSE 0 END),"
                      " SUM(CASE WHEN method = 'face' THEN amount ELSE 0 END)"
                      " FROM sales WHERE sold_at >= ? AND sold_at < ?"
                      " GROUP BY bucket ORDER BY bucket")
                  .arg(mon ? "DAY(sold_at)" : "HOUR(sold_at)"));
    q.addBindValue(from);
    q.addBindValue(to);
    if (!q.exec())
        return fail(q, err);
    QVariantList buckets;
    while (q.next()) {
        QVariantMap m;
        m["bucket"] = q.value(0).toInt();
        m["card"] = q.value(1).toInt();
        m["face"] = q.value(2).toInt();
        buckets << m;
    }

    q.prepare("SELECT s.sold_at, COALESCE(u.name, '(알 수 없음)'), " SOLD_NAME("s.") ", s.qty, s.amount, s.method"
              " FROM sales s LEFT JOIN users u ON u.id = s.user_id"
              " WHERE s.sold_at >= ? AND s.sold_at < ?"
              " ORDER BY s.sold_at DESC, s.id DESC");
    q.addBindValue(from);
    q.addBindValue(to);
    if (!q.exec())
        return fail(q, err);
    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["soldAt"] = q.value(0).toDateTime().toString("yyyy-MM-dd HH:mm:ss");
        m["buyer"] = q.value(1).toString();
        m["product"] = q.value(2).toString();
        m["qty"] = q.value(3).toInt();
        m["amount"] = q.value(4).toInt();
        m["method"] = q.value(5).toString();
        list << m;
    }

    QVariantMap res;
    res["buckets"] = buckets;
    res["details"] = list;
    *out = res;
    return true;
}

// from~to 기간을 바인딩해 쿼리 실행
static bool range(QSqlQuery &q, const QString &sql, const QDateTime &from, const QDateTime &to)
{
    q.prepare(sql);
    q.addBindValue(from);
    q.addBindValue(to);
    return q.exec();
}

// 일반 회원 목록 (AI 상담에서 회원 선택용)
bool DB::members(QVariant *out, QString *err)
{
    QSqlQuery q;
    if (!q.exec("SELECT id, name FROM users WHERE role = 'member' ORDER BY name"))
        return fail(q, err);
    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0).toInt();
        m["name"] = q.value(1).toString();
        list << m;
    }
    *out = list;
    return true;
}

// 기간의 매출 요약: 합계/수량/주문 수, 상품별, 시간대별, 구매자별 (AI 리포트용)
bool DB::summary(const QDateTime &from, const QDateTime &to, QVariantMap *out, QString *err)
{
    QSqlQuery q;
    QVariantMap res;
    if (!range(q, "SELECT COALESCE(SUM(amount), 0), COALESCE(SUM(qty), 0), COUNT(DISTINCT sold_at),"
                  " COALESCE(SUM(CASE WHEN method = 'card' THEN amount END), 0), COUNT(DISTINCT user_id)"
                  " FROM sales WHERE sold_at >= ? AND sold_at < ?", from, to) || !q.next())
        return fail(q, err);
    res["from"] = from.toString("yyyy-MM-dd HH:mm");
    res["to"] = to.toString("yyyy-MM-dd HH:mm");
    res["total"] = q.value(0).toInt();
    res["qty"] = q.value(1).toInt();
    res["orders"] = q.value(2).toInt();
    res["card"] = q.value(3).toInt();
    res["face"] = q.value(0).toInt() - q.value(3).toInt();
    res["customers"] = q.value(4).toInt();

    if (!range(q, "SELECT " SOLD_NAME("") " AS name, SUM(qty), SUM(amount) FROM sales"
                  " WHERE sold_at >= ? AND sold_at < ? GROUP BY name ORDER BY SUM(qty) DESC", from, to))
        return fail(q, err);
    QVariantList products;
    while (q.next()) {
        QVariantMap m;
        m["name"] = q.value(0).toString();
        m["qty"] = q.value(1).toInt();
        m["amount"] = q.value(2).toInt();
        products << m;
    }
    res["products"] = products;

    if (!range(q, "SELECT HOUR(sold_at), SUM(amount) FROM sales"
                  " WHERE sold_at >= ? AND sold_at < ? GROUP BY HOUR(sold_at) ORDER BY 1", from, to))
        return fail(q, err);
    QVariantList hours;
    while (q.next()) {
        QVariantMap m;
        m["hour"] = q.value(0).toInt();
        m["amount"] = q.value(1).toInt();
        hours << m;
    }
    res["hours"] = hours;

    if (!range(q, "SELECT COALESCE(u.name, '(비회원)'), COUNT(DISTINCT s.sold_at), SUM(s.amount), MAX(s.sold_at)"
                  " FROM sales s LEFT JOIN users u ON u.id = s.user_id"
                  " WHERE s.sold_at >= ? AND s.sold_at < ?"
                  " GROUP BY s.user_id, u.name ORDER BY SUM(s.amount) DESC LIMIT 10", from, to))
        return fail(q, err);
    QVariantList buyers;
    while (q.next()) {
        QVariantMap m;
        m["name"] = q.value(0).toString();
        m["visits"] = q.value(1).toInt();
        m["amount"] = q.value(2).toInt();
        m["last"] = q.value(3).toDateTime().toString("yyyy-MM-dd HH:mm");
        buyers << m;
    }
    res["buyers"] = buyers;

    *out = res;
    return true;
}

// from 부터 days 일 동안의 일별 판매 수량 (상품 0 이면 전체)
bool DB::daily(int productId, const QDate &from, int days, QVector<int> *qty, QString *err)
{
    QSqlQuery q;
    q.prepare(QString("SELECT DATE(sold_at), SUM(qty) FROM sales"
                      " WHERE sold_at >= ? AND sold_at < ? %1 GROUP BY DATE(sold_at)")
                  .arg(productId ? "AND product_id = ?" : ""));
    q.addBindValue(QDateTime(from));
    q.addBindValue(QDateTime(from.addDays(days)));
    if (productId)
        q.addBindValue(productId);
    if (!q.exec())
        return fail(q, err);
    qty->fill(0, days);
    while (q.next()) {
        int i = from.daysTo(q.value(0).toDate());
        if (i >= 0 && i < days)
            (*qty)[i] = q.value(1).toInt();
    }
    return true;
}

// 회원의 최근 days 일 상품별 구매 기록 (AI 의 회원 구매 성향 분석용)
bool DB::purchases(int userId, int days, QVariantList *out, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT " SOLD_NAME("") " AS name, SUM(qty), COUNT(DISTINCT DATE(sold_at)), MAX(sold_at),"
              " GROUP_CONCAT(DISTINCT HOUR(sold_at))"
              " FROM sales WHERE user_id = ? AND sold_at >= ?"
              " GROUP BY name ORDER BY SUM(qty) DESC");
    q.addBindValue(userId);
    q.addBindValue(QDateTime::currentDateTime().addDays(-days));
    if (!q.exec())
        return fail(q, err);
    out->clear();
    while (q.next()) {
        QVariantMap m;
        m["product"] = q.value(0).toString();
        m["qty"] = q.value(1).toInt();
        m["days"] = q.value(2).toInt();
        m["last"] = q.value(3).toDateTime().toString("yyyy-MM-dd HH:mm");
        m["hours"] = q.value(4).toString();
        *out << m;
    }
    return true;
}
