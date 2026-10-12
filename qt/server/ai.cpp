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

static const char *GROQ_URL = "https://api.groq.com/openai/v1/chat/completions";

static QString groqKey;
static QStringList models;

// server.ini 의 [ai] 를 읽어 Groq 키와 시도할 모델 순서 결정 (기본 모델 → 예비 모델)
void AI::setup(const QSettings &ini)
{
    groqKey = ini.value("ai/groq_key").toString().trimmed();
    models.clear();
    QString main = ini.value("ai/groq_model", "openai/gpt-oss-120b").toString().trimmed();
    QString backup = ini.value("ai/groq_fallback_model", "openai/gpt-oss-20b").toString().trimmed();
    if (!main.isEmpty())
        models << main;
    if (!backup.isEmpty() && backup != main)
        models << backup;
}

// Groq 요청 본문 만들기, schema 가 있으면 그 JSON 형식으로만 답하게 함
static QJsonObject makeBody(const QString &model, const QString &system, const QString &prompt, const QJsonObject &schema)
{
    QJsonObject body;
    body["model"] = model;
    body["messages"] = QJsonArray{QJsonObject{{"role", "system"}, {"content", system}},
                                  QJsonObject{{"role", "user"}, {"content", prompt}}};
    body["reasoning_effort"] = "low";
    body["include_reasoning"] = false;
    if (!schema.isEmpty())
        body["response_format"] = QJsonObject{
            {"type", "json_schema"},
            {"json_schema", QJsonObject{{"name", "answer"}, {"strict", true}, {"schema", schema}}}};
    return body;
}

// Groq API 에 요청을 보내고 HTTP 상태 코드를 돌려줌 (시간 초과면 0)
static int post(const QJsonObject &body, int timeout, QJsonObject *res, QString *err)
{
    QNetworkRequest req(QUrl(QString::fromLatin1(GROQ_URL)));
    req.setRawHeader("Authorization", "Bearer " + groqKey.toUtf8());
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
    if (status == 401)
        *err = "API 키가 올바르지 않습니다. server.ini 의 groq_key 를 확인하세요.";
    else if (status == 429)
        *err = "사용량 한도 초과";
    else if (status == 500 || status == 502 || status == 503)
        *err = "서버 혼잡";
    else if (status != 200)
        *err = res->value("error").toObject().value("message").toString(rep->errorString());
    return status;
}

// 응답에서 답 글자만 꺼냄 (빈 답이면 오류)
static bool readAnswer(const QJsonObject &res, QString *answer, QString *err)
{
    QJsonObject first = res.value("choices").toArray().at(0).toObject();
    *answer = first.value("message").toObject().value("content").toString();
    if (answer->trimmed().isEmpty()) {
        *err = "빈 답변 (" + first.value("finish_reason").toString() + ")";
        return false;
    }
    return true;
}

// 다시 시도해 볼 만한 오류인지 (시간 초과, 서버 혼잡)
static bool retry(int status)
{
    return status == 0 || status == 500 || status == 502 || status == 503;
}

// Groq 에 질문: 기본 모델 → 예비 모델 순서로 시도하고(모델마다 최대 2번) 처음 성공한 답을 돌려줌
bool AI::ask(const QString &system, const QString &prompt, const QJsonObject &schema, QString *answer, QString *err)
{
    if (groqKey.isEmpty() || models.isEmpty()) {
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
    foreach (const QString &model, models) {
        QJsonObject body = makeBody(model, system, prompt, schema);
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
            int status = post(body, qMin(left, ATTEMPT), &res, &msg);
            if (status == 200 && readAnswer(res, answer, &msg))
                return true;
            if (!retry(status))
                break;
        }
        errors << QString("%1: %2").arg(model, msg);
    }
    *err = "AI 답변을 받지 못했습니다. 잠시 후 다시 시도하세요.\n" + errors.join("\n");
    return false;
}
