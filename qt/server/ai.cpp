#include "ai.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QSslSocket>
#include <QTimer>

static const int TIMEOUT = 60000;
static QString apiKey;
static QString model;

void AI::setup(const QSettings &ini)
{
    apiKey = ini.value("ai/api_key").toString();
    model = ini.value("ai/model", "gemini-2.5-flash").toString();
}

static QJsonObject textPart(const QString &text)
{
    return QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", text}}}}};
}

// Gemini API (generateContent) 호출
bool AI::ask(const QString &system, const QString &prompt, const QJsonObject &schema,QString *answer, QString *err)
{
    if (apiKey.isEmpty()) {
        *err = "server.ini 의 [ai] api_key 가 비어 있습니다.";
        return false;
    }
    if (!QSslSocket::supportsSsl()) {
        *err = "HTTPS를 사용할 수 없습니다. OpenSSL 라이브러리를 설치하세요.\n"
               "Ubuntu: sudo apt install libssl1.0.0 libssl1.1";
        return false;
    }
    QJsonObject config;
    if (!schema.isEmpty()) {
        config["responseMimeType"] = "application/json";
        config["responseSchema"] = schema;
    }
    QJsonObject user = textPart(prompt);
    user["role"] = "user";
    QJsonObject body;
    body["systemInstruction"] = textPart(system);
    body["contents"] = QJsonArray{user};
    body["generationConfig"] = config;
    QNetworkRequest req(QUrl(QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent").arg(model)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-goog-api-key", apiKey.toUtf8());
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
    QJsonArray candidates = res.value("candidates").toArray();
    if (candidates.isEmpty()) {
        *err = "AI가 답변을 거절했습니다. (" + res.value("promptFeedback").toObject().value("blockReason").toString() + ")";
        return false;
    }
    QJsonObject first = candidates.at(0).toObject();
    answer->clear();
    foreach (const QJsonValue &v, first.value("content").toObject().value("parts").toArray()) {
        QJsonObject part = v.toObject();
        if (!part.value("thought").toBool())
            *answer += part.value("text").toString();
    }
    if (answer->isEmpty()) {
        *err = "AI 답변이 비어 있습니다. (" + first.value("finishReason").toString() + ")";
        return false;
    }
    return true;
}
