#ifndef SERVER_H
#define SERVER_H

#include <QHash>
#include <QJsonObject>
#include <QQueue>
#include <QSemaphore>
#include <QSet>
#include <QTcpServer>
#include <QThread>
#include <QVariant>

class QTcpSocket;
class DB;

class Server : public QTcpServer
{
    Q_OBJECT
public:
    explicit Server(DB *db, QObject *parent = 0);

    void setAdminCode(const QString &code) { m_adminCode = code; }
    void setTossKey(const QString &key) { m_tossKey = key; }

private slots:
    void onConnect();
    void onRead();
    void onClose();

private:

    QJsonObject handle(const QJsonObject &req, bool local);
    bool checkAdminCode(const QString &code, QString *err);
    QString newToken(int userId);
    bool checkAdmin(const QJsonObject &req, QString *err);

    DB *m_db;
    QHash<QTcpSocket *, QByteArray> m_buf;
    QString m_adminCode;
    QString m_tossKey;              // 토스 결제 승인용 시크릿 키
    QSet<QTcpSocket *> m_busy;      // 요청을 처리하는 중인 연결 (토스·AI 응답 대기 중 다시 읽지 않도록)
    QHash<QString, int> m_tokens;   // 로그인 토큰 -> 회원 번호
    QQueue<QString> m_tokenOrder;   // 오래된 토큰부터 지우기 위한 발급 순서
};

class ServerThread : public QThread
{
public:
    ~ServerThread();
    bool startWait(QString *err);

protected:
    void run();

private:
    QSemaphore m_ready;
    bool m_ok;
    QString m_err;
};

class Esp : public QTcpServer
{
    Q_OBJECT
public:
    explicit Esp(QObject *parent = 0);
    ~Esp();

    static Esp *get();

    bool start(QString *err);
    bool connected() const;
    bool present() const { return m_present; }
    QVariantList shelf() const { return m_shelf; }
    static bool shelfEmpty(int shelf);

    void buzzer();
    void cardSelect();
    void cardFail();
    void cardCancel();

signals:
    void presence(bool on);
    void card(const QString &uid);
    void shelfChanged(const QVariantList &shelf);

private slots:
    void onConnect();
    void onRead();
    void onClose();

private:
    void handle(const QString &line);
    void send(const QString &cmd);

    QTcpSocket *m_sock;
    QByteArray m_buf;
    bool m_present;
    QVariantList m_shelf;
};

#endif
