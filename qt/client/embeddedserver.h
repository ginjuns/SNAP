#ifndef EMBEDDEDSERVER_H
#define EMBEDDEDSERVER_H

#include <QSemaphore>
#include <QString>
#include <QThread>

// 키오스크 안에서 9000번 포트 서버를 별도 스레드로 돌린다.
// 화면 쪽 ServerClient가 동기 방식으로 기다리므로 같은 스레드에 두면 응답을 못 한다.
class EmbeddedServer : public QThread
{
public:
    ~EmbeddedServer();
    // 서버를 시작하고 DB 연결/포트 열기 결과가 나올 때까지 기다린다.
    bool startAndWait(QString *err);

protected:
    void run();

private:
    QSemaphore m_ready;
    bool m_ok;
    QString m_err;
};

#endif // EMBEDDEDSERVER_H
