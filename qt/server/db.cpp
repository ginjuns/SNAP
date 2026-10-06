#include "db.h"
#include "packet.h"

#include <QtSql>
#include <QDateTime>
#include <QSettings>

static bool fail(const QSqlQuery &q, QString *err)
{
    *err = q.lastError().text();
    return false;
}

static bool rollback(const QString &msg, QString *err)
{
    QSqlDatabase::database().rollback();
    *err = msg;
    return false;
}

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

static bool shelfValue(int shelf, QVariant *v, QString *err)
{
    if (shelf < 0 || shelf > SHELF_COUNT) {
        *err = QString("진열대 번호는 1~%1 이어야 합니다.").arg(SHELF_COUNT);
        return false;
    }
    *v = shelf ? QVariant(shelf) : QVariant(QVariant::Int);
    return true;
}

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
    return true;
}

bool DB::join(const QString &name0, const QString &id0, const QString &pw,
              const QList<QByteArray> &faces, QVariant *out, QString *err)
{
    QString name = name0.trimmed();
    QString id = id0.trimmed();
    if (name.isEmpty() || id.isEmpty() || pw.isEmpty()) {
        *err = "이름, 아이디, 비밀번호를 모두 입력하세요.";
        return false;
    }
    if (faces.isEmpty()) {
        *err = "얼굴 사진이 없습니다. (faces에 1장 이상)";
        return false;
    }

    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM users WHERE login_id = ?");
    q.addBindValue(id);
    if (!q.exec() || !q.next())
        return fail(q, err);
    if (q.value(0).toInt() > 0) {
        *err = QString("'%1' 아이디는 이미 사용 중입니다.").arg(id);
        return false;
    }

    QSqlDatabase::database().transaction();
    q.prepare("INSERT INTO users (name, role, login_id, password) VALUES (?, 'member', ?, ?)");
    q.addBindValue(name);
    q.addBindValue(id);
    q.addBindValue(pw);
    if (!q.exec())
        return rollback(q.lastError().text(), err);
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
    *out = m;
    return true;
}

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

bool DB::login(const QString &id, const QString &pw, QVariant *out, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT id FROM users WHERE login_id = ? AND password = ?");
    q.addBindValue(id);
    q.addBindValue(pw);
    if (!q.exec())
        return fail(q, err);
    if (!q.next()) {
        *err = "아이디 또는 비밀번호가 올바르지 않습니다.";
        return false;
    }
    return user(q.value(0).toInt(), out, err);
}

bool DB::user(int id, QVariant *out, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT id, name, role, balance FROM users WHERE id = ?");
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
    *out = m;
    return true;
}

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

    q.prepare("SELECT s.sold_at, COALESCE(u.name, '(알 수 없음)'), s.product_name, s.qty, s.amount, s.method"
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

static bool range(QSqlQuery &q, const QString &sql, const QDateTime &from, const QDateTime &to)
{
    q.prepare(sql);
    q.addBindValue(from);
    q.addBindValue(to);
    return q.exec();
}

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

bool DB::summary(const QDateTime &from, const QDateTime &to, QVariantMap *out, QString *err)
{
    QSqlQuery q;
    QVariantMap res;
    if (!range(q, "SELECT COALESCE(SUM(amount), 0), COALESCE(SUM(qty), 0), COUNT(DISTINCT sold_at),"
                  " COALESCE(SUM(CASE WHEN method = 'card' THEN amount END), 0)"
                  " FROM sales WHERE sold_at >= ? AND sold_at < ?", from, to) || !q.next())
        return fail(q, err);
    res["from"] = from.toString("yyyy-MM-dd HH:mm");
    res["to"] = to.toString("yyyy-MM-dd HH:mm");
    res["total"] = q.value(0).toInt();
    res["qty"] = q.value(1).toInt();
    res["orders"] = q.value(2).toInt();
    res["card"] = q.value(3).toInt();
    res["face"] = q.value(0).toInt() - q.value(3).toInt();

    if (!range(q, "SELECT product_name, SUM(qty), SUM(amount) FROM sales"
                  " WHERE sold_at >= ? AND sold_at < ? GROUP BY product_name ORDER BY SUM(qty) DESC", from, to))
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

bool DB::purchases(int userId, int days, QVariantList *out, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT product_name, SUM(qty), COUNT(DISTINCT DATE(sold_at)), MAX(sold_at),"
              " GROUP_CONCAT(DISTINCT HOUR(sold_at))"
              " FROM sales WHERE user_id = ? AND sold_at >= ?"
              " GROUP BY product_name ORDER BY SUM(qty) DESC");
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
