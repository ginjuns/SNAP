#ifndef SERVER_H
#define SERVER_H

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

private slots:
    void onConnect();
    void onRead();
    void onClose();

private:
    QJsonObject handle(const QJsonObject &req);

    DB *m_db;
    QHash<QTcpSocket *, QByteArray> m_buf;
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

    void buzzer();

signals:
    void presence(bool on);
    void card(const QString &uid);
    void shelfChanged(const QVariantList &shelf);

private slots:
    void onConnect();
    void onRead();
    void onClose();

private:
    void handle(const QJsonObject &req);
    void send(const QJsonObject &obj);

    QTcpSocket *m_sock;
    QByteArray m_buf;
    bool m_present;
    QVariantList m_shelf;
};

#endif
