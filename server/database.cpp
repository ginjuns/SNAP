#include "database.h"

#include <QtSql>
#include <QDateTime>

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

bool Database::open(const QString &path, QString *err)
{
    // MySQL을 쓰려면 QMYSQL로 바꾸고 setHostName/setUserName/setPassword를 지정한다.
    // (CREATE TABLE 문도 MySQL 문법에 맞게 AUTO_INCREMENT 등을 추가해야 함)
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path);
    if (!db.open()) {
        *err = db.lastError().text();
        return false;
    }

    QSqlQuery q;
    const char *ddl[] = {
        "CREATE TABLE IF NOT EXISTS users ("
        " id INTEGER PRIMARY KEY, name VARCHAR(50) NOT NULL,"
        " role VARCHAR(10) NOT NULL,"                 // 'admin' | 'member'
        " balance INTEGER NOT NULL DEFAULT 0)",       // 충전 금액
        "CREATE TABLE IF NOT EXISTS products ("
        " id INTEGER PRIMARY KEY, name VARCHAR(100) NOT NULL,"
        " price INTEGER NOT NULL, image BLOB)",
        "CREATE TABLE IF NOT EXISTS sales ("
        " id INTEGER PRIMARY KEY, user_id INTEGER, product_id INTEGER,"
        " qty INTEGER, amount INTEGER,"
        " method VARCHAR(10),"                        // 'card' | 'face'
        " sold_at VARCHAR(19))"                       // yyyy-MM-dd HH:mm:ss
    };
    for (int i = 0; i < 3; ++i)
        if (!q.exec(ddl[i]))
            return fail(q, err);

    // 최초 실행 시 샘플 데이터 (users.id = 얼굴 학습 폴더 이름 faces/<id>)
    q.exec("SELECT COUNT(*) FROM users");
    if (q.next() && q.value(0).toInt() == 0) {
        q.exec("INSERT INTO users VALUES (1, '관리자', 'admin', 0)");
        q.exec("INSERT INTO users VALUES (2, '홍길동', 'member', 50000)");
        q.exec("INSERT INTO users VALUES (3, '김철수', 'member', 30000)");
        q.exec("INSERT INTO products (name, price) VALUES ('아메리카노', 3000)");
        q.exec("INSERT INTO products (name, price) VALUES ('카페라떼', 3500)");
        q.exec("INSERT INTO products (name, price) VALUES ('샌드위치', 5000)");
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
    m["id"] = q.value(0);
    m["name"] = q.value(1);
    m["role"] = q.value(2);
    m["balance"] = q.value(3);
    *out = m;
    return true;
}

bool Database::products(QVariant *out, QString *err)
{
    QSqlQuery q;
    if (!q.exec("SELECT id, name, price, image FROM products ORDER BY id"))
        return fail(q, err);
    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["id"] = q.value(0);
        m["name"] = q.value(1);
        m["price"] = q.value(2);
        m["image"] = q.value(3).toByteArray();
        list << m;
    }
    *out = list;
    return true;
}

bool Database::addProduct(const QString &name, int price, const QByteArray &image, QString *err)
{
    if (name.trimmed().isEmpty() || price <= 0) {
        *err = "상품명과 가격을 올바르게 입력하세요.";
        return false;
    }
    QSqlQuery q;
    q.prepare("INSERT INTO products (name, price, image) VALUES (?, ?, ?)");
    q.addBindValue(name.trimmed());
    q.addBindValue(price);
    q.addBindValue(image);
    return q.exec() || fail(q, err);
}

// 매출 기록(sales)은 금액을 따로 저장하므로 상품을 지워도 매출 집계는 유지된다.
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
    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    int total = 0;

    foreach (const QVariant &v, items) {
        const QVariantMap item = v.toMap();
        const int productId = item.value("productId").toInt();
        const int qty = item.value("qty").toInt();
        q.prepare("SELECT price FROM products WHERE id = ?");
        q.addBindValue(productId);
        if (qty <= 0 || !q.exec() || !q.next())
            return rollback("잘못된 상품 정보입니다.", err);

        const int amount = q.value(0).toInt() * qty;
        total += amount;

        q.prepare("INSERT INTO sales (user_id, product_id, qty, amount, method, sold_at) VALUES (?, ?, ?, ?, ?, ?)");
        q.addBindValue(userId);
        q.addBindValue(productId);
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
        q.prepare("SELECT balance FROM users WHERE id = ?");
        q.addBindValue(userId);
        if (!q.exec() || !q.next())
            return rollback("등록되지 않은 사용자입니다.", err);

        const int balance = q.value(0).toInt();
        if (balance < total)
            return rollback(QString("잔액이 부족합니다. (잔액 %1원 / 결제금액 %2원)").arg(balance).arg(total), err);

        q.prepare("UPDATE users SET balance = balance - ? WHERE id = ?");
        q.addBindValue(total);
        q.addBindValue(userId);
        if (!q.exec())
            return rollback(q.lastError().text(), err);
        result["balance"] = balance - total;
    }

    if (!QSqlDatabase::database().commit())
        return rollback(QSqlDatabase::database().lastError().text(), err);
    *out = result;
    return true;
}

// unit: "day" -> yyyy-MM-dd 단위 합계, "month" -> yyyy-MM 단위 합계
bool Database::sales(const QString &unit, QVariant *out, QString *err)
{
    const int len = (unit == "month") ? 7 : 10;
    QSqlQuery q;
    if (!q.exec(QString("SELECT SUBSTR(sold_at, 1, %1) AS period, SUM(qty),"
                        " SUM(CASE WHEN method = 'card' THEN amount ELSE 0 END),"
                        " SUM(CASE WHEN method = 'face' THEN amount ELSE 0 END),"
                        " SUM(amount)"
                        " FROM sales GROUP BY period ORDER BY period DESC").arg(len)))
        return fail(q, err);

    QVariantList list;
    while (q.next()) {
        QVariantMap m;
        m["period"] = q.value(0);
        m["qty"] = q.value(1);
        m["card"] = q.value(2);
        m["face"] = q.value(3);
        m["total"] = q.value(4);
        list << m;
    }
    *out = list;
    return true;
}
