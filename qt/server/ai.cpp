#include "ai.h"
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QSslSocket>
#include <QStringList>
#include <QThread>
#include <QTimer>

static const int DEADLINE = 70000;
static const int ATTEMPT = 35000;

struct Target
{
    bool gemini;
    QString model;
};

static QString groqKey;
static QString geminiKey;
static QList<Target> targets;

void AI::setup(const QSettings &ini)
{
    groqKey = ini.value("ai/groq_key").toString();
    geminiKey = ini.value("ai/gemini_key").toString();
    targets.clear();
    if (!groqKey.isEmpty()) {
        Target main = {false, ini.value("ai/groq_model", "openai/gpt-oss-120b").toString()};
        Target backup = {false, ini.value("ai/groq_fallback_model", "openai/gpt-oss-20b").toString()};
        targets << main;
        if (!backup.model.isEmpty() && backup.model != main.model)
            targets << backup;
    }
    if (!geminiKey.isEmpty()) {
        Target gemini = {true, ini.value("ai/gemini_model", "gemini-3.5-flash-lite").toString()};
        targets << gemini;
    }
}

static QJsonObject makeBody(const Target &t, const QString &system, const QString &prompt, const QJsonObject &schema)
{
    QJsonObject body;
    if (t.gemini) {
        QJsonObject config;
        if (!schema.isEmpty()) {
            config["responseMimeType"] = "application/json";
            config["responseJsonSchema"] = schema;
        }
        config["thinkingConfig"] = QJsonObject{{"thinkingLevel", "low"}};
        body["systemInstruction"] = QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", system}}}}};
        body["contents"] = QJsonArray{QJsonObject{{"role", "user"}, {"parts", QJsonArray{QJsonObject{{"text", prompt}}}}}};
        body["generationConfig"] = config;
    } else {
        body["model"] = t.model;
        body["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", system + "\n\n" + prompt}}};
        body["reasoning_effort"] = "low";
        body["include_reasoning"] = false;
        if (!schema.isEmpty())
            body["response_format"] = QJsonObject{
                {"type", "json_schema"},
                {"json_schema", QJsonObject{{"name", "answer"}, {"strict", true}, {"schema", schema}}}};
    }
    return body;
}

static int post(const Target &t, const QJsonObject &body, int timeout, QJsonObject *res, QString *err)
{
    QNetworkRequest req;
    if (t.gemini) {
        req.setUrl(QUrl(QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent").arg(t.model)));
        req.setRawHeader("x-goog-api-key", geminiKey.toUtf8());
    } else {
        req.setUrl(QUrl("https://api.groq.com/openai/v1/chat/completions"));
        req.setRawHeader("Authorization", "Bearer " + groqKey.toUtf8());
    }
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkAccessManager net;
    QNetworkReply *rep = net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(rep, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(timeout);
    loop.exec();
    if (!rep->isFinished()) {
        rep->abort();
        *err = "응답 시간 초과";
        return 0;
    }
    *res = QJsonDocument::fromJson(rep->readAll()).object();
    int status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 429)
        *err = "사용량 한도 초과";
    else if (status == 500 || status == 502 || status == 503)
        *err = "서버 혼잡";
    else if (status != 200)
        *err = res->value("error").toObject().value("message").toString(rep->errorString());
    return status;
}

static bool readAnswer(const Target &t, const QJsonObject &res, QString *answer, QString *err)
{
    answer->clear();
    QString reason;
    if (t.gemini) {
        QJsonArray candidates = res.value("candidates").toArray();
        if (candidates.isEmpty()) {
            *err = "답변 거절 (" + res.value("promptFeedback").toObject().value("blockReason").toString() + ")";
            return false;
        }
        QJsonObject first = candidates.at(0).toObject();
        foreach (const QJsonValue &v, first.value("content").toObject().value("parts").toArray()) {
            QJsonObject part = v.toObject();
            if (!part.value("thought").toBool())
                *answer += part.value("text").toString();
        }
        reason = first.value("finishReason").toString();
    } else {
        QJsonObject first = res.value("choices").toArray().at(0).toObject();
        *answer = first.value("message").toObject().value("content").toString();
        reason = first.value("finish_reason").toString();
    }
    if (answer->trimmed().isEmpty()) {
        *err = "빈 답변 (" + reason + ")";
        return false;
    }
    return true;
}

static bool retry(int status)
{
    return status == 0 || status == 500 || status == 502 || status == 503;
}

bool AI::ask(const QString &system, const QString &prompt, const QJsonObject &schema,QString *answer, QString *err)
{
    if (targets.isEmpty()) {
        *err = "server.ini 의 [ai] groq_key 가 비어 있습니다.";
        return false;
    }
    if (!QSslSocket::supportsSsl()) {
        *err = "HTTPS를 사용할 수 없습니다. OpenSSL 라이브러리를 설치하세요.\n"
               "Ubuntu: sudo apt install libssl1.0.0 libssl1.1";
        return false;
    }

    QElapsedTimer clock;
    clock.start();
    QStringList errors;
    foreach (const Target &t, targets) {
        QJsonObject body = makeBody(t, system, prompt, schema);
        QString msg;
        for (int i = 0; i < 2; ++i) {
            if (i > 0)
                QThread::msleep(1000);
            int left = DEADLINE - (int)clock.elapsed();
            if (left <= 0) {
                msg = "응답 시간 초과";
                break;
            }
            QJsonObject res;
            int status = post(t, body, qMin(left, ATTEMPT), &res, &msg);
            if (status == 200 && readAnswer(t, res, answer, &msg))
                return true;
            if (!retry(status))
                break;
        }
        errors << QString("%1: %2").arg(t.model, msg);
    }
    *err = "AI 답변을 받지 못했습니다. 잠시 후 다시 시도하세요.\n" + errors.join("\n");
    return false;
}
