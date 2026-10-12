#ifndef SERVER_H
#define SERVER_H

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QSemaphore>
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

private slots:
    void onConnect();
    void onRead();
    void onClose();

private:
    struct AdminTry {
        int fails;
        QDateTime lockedUntil;
    };

    QJsonObject handle(const QJsonObject &req, const QString &peer);
    bool checkAdminCode(const QString &code, const QString &peer, QString *err);

    DB *m_db;
    QHash<QTcpSocket *, QByteArray> m_buf;
    QString m_adminCode;
    QHash<QString, AdminTry> m_adminTries;   // IP별 관리자 코드 오답 횟수
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
