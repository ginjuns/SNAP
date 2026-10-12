#ifndef NET_H
#define NET_H

#include <QString>
#include <QVariant>

class Net
{
public:
    static void setHost(const QString &ip);
    static bool call(const QString &cmd, const QVariantMap &args, QVariant *out, QString *err);
};

inline QString won(int v)
{
    return QString("%L1원").arg(v);
}

#endif
