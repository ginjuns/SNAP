#ifndef DATABASE_H
#define DATABASE_H

#include <QString>
#include <QVariant>

class QSettings;

// MySQL 접근. 모든 함수는 성공 시 true, 실패 시 false와 함께 err에 사유를 담는다.
class Database
{
public:
    bool open(const QSettings &config, QString *err);

    bool user(int id, QVariant *out, QString *err);
    // 아이디/비밀번호가 맞으면 user()와 같은 사용자 정보를 돌려준다.
    bool userByPassword(const QString &loginId, const QString &password, QVariant *out, QString *err);
    // 앱 회원가입: 사용자와 얼굴 사진을 함께 저장한다. out = { userId }
    bool registerUser(const QString &name, const QString &loginId, const QString &password,
                      const QList<QByteArray> &faces, QVariant *out, QString *err);
    // afterId보다 뒤에 저장된 얼굴 사진. out = [{ id, userId, image(Base64) }]
    bool faces(int afterId, QVariant *out, QString *err);
    bool products(QVariant *out, QString *err);
    bool addProduct(const QString &name, int price, int stock, const QByteArray &image, QString *err);
    // fields: name / price / stock / image 중 바꿀 항목만 담는다.
    bool updateProduct(int id, const QVariantMap &fields, QString *err);
    bool deleteProduct(int id, QString *err);
    bool pay(int userId, const QString &method, const QVariantList &items, QVariant *out, QString *err);
    bool sales(const QString &unit, const QString &date, QVariant *out, QString *err);
};

#endif // DATABASE_H
