#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QHash>
#include <QJsonObject>

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

#endif
