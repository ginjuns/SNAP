#ifndef SERVERCLIENT_H
#define SERVERCLIENT_H

#include <QString>
#include <QVariant>

// 9000번 포트 서버에 요청 하나를 보내고 응답을 기다린다(동기 방식).
// 예) ServerClient::call("LOGIN", QVariantMap{{"userId", 2}}, &data, &err);
class ServerClient
{
public:
    static void setHost(const QString &host);
    static bool call(const QString &cmd, const QVariantMap &args, QVariant *result, QString *err);
};

// 12000 -> "12,000원"
inline QString won(int value)
{
    return QString("%L1원").arg(value);
}

#endif // SERVERCLIENT_H
