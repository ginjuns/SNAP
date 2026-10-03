#ifndef ESP_H
#define ESP_H

#include <QTcpServer>
#include <QJsonObject>
#include <QVariant>

class QTcpSocket;

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
