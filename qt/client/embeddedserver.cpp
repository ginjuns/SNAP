#include "embeddedserver.h"
#include "database.h"
#include "kioskserver.h"
#include "protocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

// server.ini 위치 찾기: 현재 폴더 -> 실행 파일 폴더 -> 소스 폴더(server/)
// Qt Creator는 빌드 폴더에서 실행하므로 소스 폴더까지 찾아본다.
static QString findConfig(QStringList *searched)
{
    const QStringList dirs = QStringList()
                             << QDir::currentPath()
                             << QCoreApplication::applicationDirPath()
                             << QString(SERVER_SOURCE_DIR);
    foreach (const QString &dir, dirs) {
        const QString path = QDir(dir).absoluteFilePath("server.ini");
        if (QFileInfo(path).isFile())
            return path;
        if (!searched->contains(path))
            *searched << path;
    }
    return QString();
}

EmbeddedServer::~EmbeddedServer()
{
    quit();
    wait();
}

bool EmbeddedServer::startAndWait(QString *err)
{
    start();
    m_ready.acquire();
    *err = m_err;
    return m_ok;
}

void EmbeddedServer::run()
{
    m_ok = false;
    QStringList searched;
    const QString configPath = findConfig(&searched);
    if (configPath.isEmpty()) {
        m_err = QString("server.ini 를 찾을 수 없습니다. server.ini.example 을 server.ini 로 복사하세요.\n찾아본 위치:\n  %1")
                    .arg(searched.join("\n  "));
        m_ready.release();
        return;
    }
    QSettings config(configPath, QSettings::IniFormat);

    Database db;
    KioskServer server(&db);
    QString dbErr;
    if (!db.open(config, &dbErr)) {
        m_err = "DB 연결 실패: " + dbErr;
    } else if (!server.listen(QHostAddress::Any, KIOSK_PORT)) {
        m_err = QString("포트 %1 열기 실패: %2").arg(KIOSK_PORT).arg(server.errorString());
    } else {
        m_ok = true;
        qDebug("키오스크 서버 시작 (포트 %d, 설정 %s)", KIOSK_PORT, qPrintable(configPath));
    }
    m_ready.release();
    if (m_ok)
        exec();
}
