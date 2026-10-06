#ifndef AI_H
#define AI_H

#include <QString>

class QSettings;

// Claude API(Messages) 호출. server.ini의 [ai] 설정을 사용한다.
class AI
{
public:
    static void setup(const QSettings &ini);
    static bool ask(const QString &system, const QString &prompt, QString *answer, QString *err);
};

#endif
