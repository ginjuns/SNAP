#ifndef SERVERTHREAD_H
#define SERVERTHREAD_H

#include <QSemaphore>
#include <QString>
#include <QThread>

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

#endif
