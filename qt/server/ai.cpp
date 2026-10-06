#include "ai.h"
#include <QElapsedTimer>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QSslSocket>
#include <QStringList>
#include <QThread>
#include <QTimer>

static const int DEADLINE = 70000;   // 재시도 포함 전체 제한 시간 (클라이언트는 90초까지 기다림)
static const int ATTEMPT = 35000;    // 요청 1번의 제한 시간 (넘으면 다시 시도)
static QString apiKey;
static QString model;
static QString fallback;
static QString thinkingLevel;

void AI::setup(const QSettings &ini)
{
    apiKey = ini.value("ai/api_key").toString();
    model = ini.value("ai/model", "gemini-3.5-flash-lite").toString();
    fallback = ini.value("ai/fallback_model", "gemini-3.8-flash").toString();
    thinkingLevel = ini.value("ai/thinking_level", "low").toString();
}

static QJsonObject textPart(const QString &text)
{
    return QJsonObject{{"parts", QJsonArray{QJsonObject{{"text", text}}}}};
}

// 요청 1번. 반환값: HTTP 상태 코드 (0 = 시간 초과). 실패했을 때만 err를 채운다.
static int post(const QString &name, const QJsonObject &body, int timeout, QJsonObject *res, QString *err)
{
    QNetworkRequest req(QUrl(QString("https://generativelanguage.googleapis.com/v1beta/models/%1:generateContent").arg(name)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("x-goog-api-key", apiKey.toUtf8());
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
        *err = "AI 응답 시간이 초과되었습니다.";
        return 0;
    }
    *res = QJsonDocument::fromJson(rep->readAll()).object();
    int status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status != 200)
        *err = res->value("error").toObject().value("message").toString(rep->errorString());
    return status;
}

// 다시 시도할 만한 실패: 시간 초과(0), 서버 오류(500), 혼잡(503)
static bool retry(int status)
{
    return status == 0 || status == 500 || status == 503;
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
    if (!thinkingLevel.isEmpty())   // 생각 단계를 줄여 빠르게 답하게 한다 (빈 값 = 모델 기본값)
        config["thinkingConfig"] = QJsonObject{{"thinkingLevel", thinkingLevel}};
    QJsonObject user = textPart(prompt);
    user["role"] = "user";
    QJsonObject body;
    body["systemInstruction"] = textPart(system);
    body["contents"] = QJsonArray{user};
    body["generationConfig"] = config;

    // 실패하면 1초 쉬고 한 번 더, 그래도 안 되면 예비 모델로 바꾼다.
    QStringList models;
    models << model;
    if (!fallback.isEmpty() && fallback != model)
        models << fallback;
    QElapsedTimer clock;
    clock.start();
    QJsonObject res;
    int status = 0;
    for (int k = 0; k < models.size(); ++k) {
        for (int i = 0; i < 2; ++i) {
            if (i > 0)
                QThread::msleep(1000);
            int left = DEADLINE - (int)clock.elapsed();
            if (left <= 0)
                break;
            status = post(models.at(k), body, qMin(left, ATTEMPT), &res, err);
            if (!retry(status))
                break;
        }
        if (!retry(status))
            break;
    }
    if (status == 500 || status == 503) {
        *err = "AI 서버에 요청이 몰려 있습니다. 잠시 후 다시 시도하세요.\n(" + *err + ")";
        return false;
    }
    if (status == 429) {
        *err = "AI 사용량 한도를 넘었습니다. 잠시 후 다시 시도하세요.\n(" + *err + ")";
        return false;
    }
    if (status != 200)
        return false;

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
