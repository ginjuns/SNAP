#ifndef AI_H
#define AI_H

#include <QJsonObject>
#include <QString>

class QSettings;

class AI
{
public:
    static void setup(const QSettings &ini);
    static bool ask(const QString &system, const QString &prompt, QString *answer, QString *err);
};

#endif
