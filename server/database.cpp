#include "database.h"

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

// 같은 이름의 다른 상품이 있는지 (collation이 _ci라 대소문자 무시)
static bool nameTaken(const QString &name, int exceptId, bool *taken, QString *err)
{
    QSqlQuery q;
    q.prepare("SELECT COUNT(*) FROM products WHERE name = ? AND id <> ?");
    q.addBindValue(name);
    q.addBindValue(exceptId);
    if (!q.exec() || !q.next())
        return fail(q, err);
    *taken = q.value(0).toInt() > 0;
    return true;
}

bool Database::open(const QSettings &config, QString *err)
{
    if (!QSqlDatabase::isDriverAvailable("QMYSQL")) {
        *err = "Qt MySQL 드라이버(QMYSQL)가 없습니다.\n"
               "Ubuntu: sudo apt install libqt5sql5-mysql";
        return false;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName(config.value("mysql/host", "127.0.0.1").toString());
    db.setPort(config.value("mysql/port", 3306).toInt());
    db.setDatabaseName(config.value("mysql/database", "kiosk").toString());
    db.setUserName(config.value("mysql/user", "kiosk").toString());
    db.setPassword(config.value("mysql/password").toString());
    db.setConnectOptions("MYSQL_OPT_RECONNECT=1");   // 오래 켜두면 끊기는 연결을 자동 복구
    if (!db.open()) {
        *err = db.lastError().text();
        return false;
    }

    const char *tables[] = { "users", "products", "sales" };
    QSqlQuery q;
    for (int i = 0; i < 3; ++i) {
        if (!q.exec(QString("SELECT 1 FROM %1 LIMIT 1").arg(tables[i]))) {
            *err = QString("'%1' 테이블을 읽을 수 없습니다. server/schema.sql을 먼저 실행하세요.\n%2")
                       .arg(tables[i]).arg(q.lastError().text());
            return false;
        }
    }
    if (!q.exec("SELECT stock FROM products LIMIT 1")) {
        *err = "products 테이블에 재고(stock) 칸이 없습니다.\n"
               "sudo mysql < server/migrate_stock.sql 을 한 번 실행하세요.";
        return false;
    }
    return true;
}

bool Database::user(int id, QVariant *out, QString *err)
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

bool Database::products(QVariant *out, QString *err)
{
    QSqlQuery q;
    if (!q.exec("SELECT id, name, price, stock, image FROM products ORDER BY id"))
        return fail(q, err);
    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0).toInt();
        m["name"] = q.value(1).toString();
        m["price"] = q.value(2).toInt();
        m["stock"] = q.value(3).toInt();
        m["image"] = QString::fromLatin1(q.value(4).toByteArray().toBase64());   // 사진 없으면 ""
        list << m;
    }
    *out = list;
    return true;
}

bool Database::addProduct(const QString &rawName, int price, int stock, const QByteArray &image, QString *err)
{
    const QString name = rawName.trimmed();
    if (name.isEmpty() || price <= 0 || stock < 0) {
        *err = "상품명, 가격, 재고를 올바르게 입력하세요.";
        return false;
    }
    bool taken = false;
    if (!nameTaken(name, 0, &taken, err))
        return false;
    if (taken) {
        *err = QString("'%1' 상품이 이미 등록되어 있습니다.\n다른 상품명을 입력하세요.").arg(name);
        return false;
    }

    QSqlQuery q;
    q.prepare("INSERT INTO products (name, price, stock, image) VALUES (?, ?, ?, ?)");
    q.addBindValue(name);
    q.addBindValue(price);
    q.addBindValue(stock);
    q.addBindValue(image.isEmpty() ? QVariant(QVariant::ByteArray) : QVariant(image));   // 사진 없으면 NULL
    return q.exec() || fail(q, err);
}

bool Database::updateProduct(int id, const QVariantMap &fields, QString *err)
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
    QVariantList values;
    if (fields.contains("name")) {
        const QString name = fields.value("name").toString().trimmed();
        bool taken = false;
        if (name.isEmpty()) {
            *err = "상품명을 입력하세요.";
            return false;
        }
        if (!nameTaken(name, id, &taken, err))
            return false;
        if (taken) {
            *err = QString("'%1' 상품이 이미 등록되어 있습니다.").arg(name);
            return false;
        }
        sets << "name = ?";
        values << name;
    }
    if (fields.contains("price")) {
        if (fields.value("price").toInt() <= 0) {
            *err = "가격을 올바르게 입력하세요.";
            return false;
        }
        sets << "price = ?";
        values << fields.value("price").toInt();
    }
    if (fields.contains("stock")) {
        if (fields.value("stock").toInt() < 0) {
            *err = "재고는 0 이상이어야 합니다.";
            return false;
        }
        sets << "stock = ?";
        values << fields.value("stock").toInt();
    }
    if (fields.contains("image")) {
        const QByteArray image = fields.value("image").toByteArray();
        sets << "image = ?";
        values << (image.isEmpty() ? QVariant(QVariant::ByteArray) : QVariant(image));   // "" = 사진 삭제
    }
    if (sets.isEmpty()) {
        *err = "변경할 항목(name, price, stock, image)이 없습니다.";
        return false;
    }

    q.prepare("UPDATE products SET " + sets.join(", ") + " WHERE id = ?");
    foreach (const QVariant &v, values)
        q.addBindValue(v);
    q.addBindValue(id);
    return q.exec() || fail(q, err);
}

// 매출 기록(sales)은 상품명과 금액을 따로 저장하므로 상품을 지워도 매출 집계는 유지된다.
bool Database::deleteProduct(int id, QString *err)
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

// 가격은 클라이언트가 보낸 값을 믿지 않고 DB에서 다시 계산한다.
bool Database::pay(int userId, const QString &method, const QVariantList &items, QVariant *out, QString *err)
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
    const QDateTime now = QDateTime::currentDateTime();
    int total = 0;

    foreach (const QVariant &v, items) {
        const QVariantMap item = v.toMap();
        const int productId = item.value("productId").toInt();
        const int qty = item.value("qty").toInt();
        q.prepare("SELECT price, name, stock FROM products WHERE id = ?");
        q.addBindValue(productId);
        if (qty <= 0 || !q.exec() || !q.next())
            return rollback("잘못된 상품 정보입니다. (삭제된 상품일 수 있습니다)", err);

        const int amount = q.value(0).toInt() * qty;
        const QString productName = q.value(1).toString();
        const int stock = q.value(2).toInt();
        total += amount;

        // 재고가 충분할 때만 차감 (동시에 결제해도 음수가 되지 않음)
        q.prepare("UPDATE products SET stock = stock - ? WHERE id = ? AND stock >= ?");
        q.addBindValue(qty);
        q.addBindValue(productId);
        q.addBindValue(qty);
        if (!q.exec())
            return rollback(q.lastError().text(), err);
        if (q.numRowsAffected() == 0)
            return rollback(QString("'%1' 재고가 부족합니다. (남은 수량 %2개)").arg(productName).arg(stock), err);

        q.prepare("INSERT INTO sales (user_id, product_id, product_name, qty, amount, method, sold_at)"
                  " VALUES (?, ?, ?, ?, ?, ?, ?)");
        q.addBindValue(userId);
        q.addBindValue(productId);
        q.addBindValue(productName);
        q.addBindValue(qty);
        q.addBindValue(amount);
        q.addBindValue(method);
        q.addBindValue(now);
        if (!q.exec())
            return rollback(q.lastError().text(), err);
    }

    QVariantMap result;
    result["total"] = total;

    if (method == "face") {
        // 잔액이 충분할 때만 차감 (동시에 결제해도 음수가 되지 않음)
        q.prepare("UPDATE users SET balance = balance - ? WHERE id = ? AND balance >= ?");
        q.addBindValue(total);
        q.addBindValue(userId);
        q.addBindValue(total);
        if (!q.exec())
            return rollback(q.lastError().text(), err);
        const bool deducted = q.numRowsAffected() > 0;

        q.prepare("SELECT balance FROM users WHERE id = ?");
        q.addBindValue(userId);
        if (!q.exec() || !q.next())
            return rollback("등록되지 않은 사용자입니다.", err);
        const int balance = q.value(0).toInt();
        if (!deducted)
            return rollback(QString("잔액이 부족합니다. (잔액 %1원 / 결제금액 %2원)").arg(balance).arg(total), err);
        result["balance"] = balance;
    }

    if (!QSqlDatabase::database().commit())
        return rollback(QSqlDatabase::database().lastError().text(), err);
    *out = result;
    return true;
}

// 선택한 기간의 매출
//   unit "day"   + date "yyyy-MM-dd" -> 그날의 시간대별(0~23시) 합계
//   unit "month" + date "yyyy-MM"    -> 그달의 일자별(1~31일) 합계
// 결과: { buckets: [{bucket, card, face}], details: [{soldAt, buyer, product, qty, amount, method}] }
bool Database::sales(const QString &unit, const QString &date, QVariant *out, QString *err)
{
    const bool month = (unit == "month");
    const QDate from = QDate::fromString(month ? date + "-01" : date, "yyyy-MM-dd");
    if (!from.isValid()) {
        *err = "날짜 형식이 잘못되었습니다. (일별: yyyy-MM-dd, 월별: yyyy-MM)";
        return false;
    }
    const QDateTime begin(from);
    const QDateTime end(month ? from.addMonths(1) : from.addDays(1));

    QSqlQuery q;
    q.prepare(QString("SELECT %1 AS bucket,"
                      " SUM(CASE WHEN method = 'card' THEN amount ELSE 0 END),"
                      " SUM(CASE WHEN method = 'face' THEN amount ELSE 0 END)"
                      " FROM sales WHERE sold_at >= ? AND sold_at < ?"
                      " GROUP BY bucket ORDER BY bucket")
                  .arg(month ? "DAY(sold_at)" : "HOUR(sold_at)"));
    q.addBindValue(begin);
    q.addBindValue(end);
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
    q.addBindValue(begin);
    q.addBindValue(end);
    if (!q.exec())
        return fail(q, err);
    QVariantList details;
    while (q.next()) {
        QVariantMap m;
        m["soldAt"] = q.value(0).toDateTime().toString("yyyy-MM-dd HH:mm:ss");
        m["buyer"] = q.value(1).toString();
        m["product"] = q.value(2).toString();
        m["qty"] = q.value(3).toInt();
        m["amount"] = q.value(4).toInt();
        m["method"] = q.value(5).toString();
        details << m;
    }

    QVariantMap result;
    result["buckets"] = buckets;
    result["details"] = details;
    *out = result;
    return true;
}
