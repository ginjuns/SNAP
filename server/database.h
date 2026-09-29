#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QVariant>

// 모든 함수는 성공 시 true, 실패 시 false와 함께 err에 사유를 담는다.
class Database
{
public:
    bool open(const QString &path, QString *err);

    bool user(int id, QVariant *out, QString *err);
    bool products(QVariant *out, QString *err);
    bool addProduct(const QString &name, int price, const QByteArray &image, QString *err);
    bool deleteProduct(int id, QString *err);
    bool pay(int userId, const QString &method, const QVariantList &items, QVariant *out, QString *err);
    bool sales(const QString &unit, QVariant *out, QString *err);
};

#endif // DATABASE_H
