#include "serverthread.h"
#include "db.h"
#include "server.h"
#include "packet.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

static QString findIni(QStringList *tried)
{
    QStringList dirs;
    dirs << QDir::currentPath() << QCoreApplication::applicationDirPath() << QString(SERVER_SOURCE_DIR);
    foreach (const QString &dir, dirs) {
        QString path = QDir(dir).absoluteFilePath("server.ini");
        if (QFileInfo(path).isFile())
            return path;
        if (!tried->contains(path))
            *tried << path;
    }
    return QString();
}

ServerThread::~ServerThread()
{
    quit();
    wait();
}

bool ServerThread::startWait(QString *err)
{
    start();
    m_ready.acquire();
    *err = m_err;
    return m_ok;
}

void ServerThread::run()
{
    m_ok = false;
    QStringList tried;
    QString path = findIni(&tried);
    if (path.isEmpty()) {
        m_err = QString("server.ini 를 찾을 수 없습니다. server.ini.example 을 server.ini 로 복사하세요.\n찾아본 위치:\n  %1")
                    .arg(tried.join("\n  "));
        m_ready.release();
        return;
    }
    QSettings ini(path, QSettings::IniFormat);

    DB db;
    Server server(&db);
    QString dbErr;
    if (!db.open(ini, &dbErr)) {
        m_err = "DB 연결 실패: " + dbErr;
    } else if (!server.listen(QHostAddress::Any, PORT)) {
        m_err = QString("포트 %1 열기 실패: %2").arg(PORT).arg(server.errorString());
    } else {
        m_ok = true;
        qDebug("키오스크 서버 시작 (포트 %d, 설정 %s)", PORT, qPrintable(path));
    }
    m_ready.release();
    if (m_ok)
        exec();
}
