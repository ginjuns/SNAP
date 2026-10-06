#include "ai.h"

#include <QSettings>

static QString apiKey;
static QString model;

void AI::setup(const QSettings &ini)
{
    apiKey = ini.value("ai/api_key").toString();
    model = ini.value("ai/model", "claude-opus-5-5").toString();
}

bool AI::ask(const QString &system, const QString &prompt, QString *answer, QString *err)
{
    // TODO: POST https://api.anthropic.com/v1/messages (QNetworkAccessManager + QEventLoop)
    Q_UNUSED(system);
    Q_UNUSED(prompt);
    Q_UNUSED(answer);
    *err = "AI 기능이 아직 구현되지 않았습니다.";
    return false;
}
