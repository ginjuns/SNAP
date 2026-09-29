#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>

#include "database.h"
#include "kioskserver.h"
#include "protocol.h"

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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QStringList searched;
    const QString configPath = findConfig(&searched);
    if (configPath.isEmpty()) {
        qCritical("server.ini 를 찾을 수 없습니다. server.ini.example 을 server.ini 로 복사하세요.\n찾아본 위치:\n  %s",
                  qPrintable(searched.join("\n  ")));
        return 1;
    }
    QSettings config(configPath, QSettings::IniFormat);
    qDebug("설정 파일: %s", qPrintable(configPath));

    Database db;
    QString err;
    if (!db.open(config, &err)) {
        qCritical("DB 연결 실패: %s", qPrintable(err));
        return 1;
    }

    KioskServer server(&db);
    if (!server.listen(QHostAddress::Any, KIOSK_PORT)) {
        qCritical("포트 %d 열기 실패: %s", KIOSK_PORT, qPrintable(server.errorString()));
        return 1;
    }
    qDebug("키오스크 서버 시작 (포트 %d, MySQL %s@%s)", KIOSK_PORT,
           qPrintable(config.value("mysql/database", "kiosk").toString()),
           qPrintable(config.value("mysql/host", "127.0.0.1").toString()));
    return app.exec();
}
