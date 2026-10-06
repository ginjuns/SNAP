#include "ai.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>

static const int TIMEOUT = 60000;
static QString apiKey;
static QString model;

void AI::setup(const QSettings &ini)
{
    apiKey = ini.value("ai/api_key").toString();
    model = ini.value("ai/model", "claude-opus-5-5").toString();
}
bool AI::ask(const QString &system, const QString &prompt, const QJsonObject &schema,QString *answer, QString *err)
{
    if (apiKey.isEmpty()) {
        *err = "server.ini 의 [ai] api_key 가 비어 있습니다.";
        return false;
    }
    QJsonObject config;
    config["effort"] = "low";
    if (!schema.isEmpty())
        config["format"] = QJsonObject{{"type", "json_schema"}, {"schema", schema}};
    QJsonObject body;
    body["model"] = model;
    body["max_tokens"] = 4000;
    body["system"] = system;
    body["output_config"] = config;
    body["fallbacks"] = "default";
    body["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", prompt}}};
    QNetworkRequest req(QUrl("https://api.anthropic.com/v1/messages"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-api-key", apiKey.toUtf8());
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setRawHeader("anthropic-beta", "server-side-fallback-2026-07-01");
    QNetworkAccessManager net;
    QNetworkReply *rep = net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(rep, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(TIMEOUT);
    loop.exec();
    if (!rep->isFinished()) {
        rep->abort();
        *err = "AI 응답 시간이 초과되었습니다.";
        return false;
    }
    int status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QJsonObject res = QJsonDocument::fromJson(rep->readAll()).object();
    if (status != 200) {
        *err = res.value("error").toObject().value("message").toString(rep->errorString());
        return false;
    }
    if (res.value("stop_reason").toString() == "refusal") {
        *err = "AI가 답변을 거절했습니다.";
        return false;
    }
    answer->clear();
    foreach (const QJsonValue &v, res.value("content").toArray()) {
        QJsonObject block = v.toObject();
        if (block.value("type").toString() == "text")
            *answer += block.value("text").toString();
    }
    return true;
}
