#ifndef KIOSKSERVER_H
#define KIOSKSERVER_H

#include <QTcpServer>
#include <QHash>
#include <QJsonObject>

class QTcpSocket;
class Database;

class KioskServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit KioskServer(Database *db, QObject *parent = 0);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    QJsonObject handle(const QJsonObject &request);

    Database *m_db;
    QHash<QTcpSocket *, QByteArray> m_buffers;   // 소켓별 수신 버퍼
};

#endif // KIOSKSERVER_H
