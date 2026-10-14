#ifndef DB_H
#define DB_H

#include <QDateTime>
#include <QString>
#include <QVariant>
#include <QVector>

class QSettings;

class DB
{
public:
    bool open(const QSettings &ini, QString *err);

    bool user(int id, QVariant *out, QString *err);
    bool login(const QString &id, const QString &pw, QVariant *out, QString *err);
    bool join(const QString &name, const QString &id, const QString &pw, const QString &phone,
              const QString &role, const QList<QByteArray> &faces, QVariant *out, QString *err);
    bool checkDup(const QString &id, const QString &phone, QVariant *out, QString *err);
    bool editAccount(const QString &id, const QString &pw, const QString &newId, const QString &newPw,
                     QVariant *out, QString *err);
    static bool checkCharge(int amount, QString *err);
    bool charged(const QString &paymentKey, bool *found, QString *err);
    bool topUp(int userId, int amount, const QString &paymentKey, const QString &orderId,
               QVariant *out, QString *err);
    bool history(int userId, const QString &from, const QString &to, QVariant *out, QString *err);
    bool faces(int after, QVariant *out, QString *err);
    bool products(QVariant *out, QString *err);
    bool addProduct(const QString &name, int price, int stock, int shelf, const QByteArray &img, QString *err);
    bool editProduct(int id, const QVariantMap &f, QString *err);
    bool delProduct(int id, QString *err);
    bool pay(int userId, const QString &method, const QString &cardUid, const QVariantList &items,
             QVariant *out, QString *err);
    bool sales(const QString &unit, const QString &date, QVariant *out, QString *err);
    bool members(QVariant *out, QString *err);
    bool summary(const QDateTime &from, const QDateTime &to, QVariantMap *out, QString *err);
    bool daily(int productId, const QDate &from, int days, QVector<int> *qty, QString *err);
    bool purchases(int userId, int days, QVariantList *out, QString *err);
};

#endif
