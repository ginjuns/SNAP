#include <QCoreApplication>
#include <QSettings>

#include "database.h"
#include "kioskserver.h"
#include "protocol.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // MySQL 접속 정보: 실행 폴더의 server.ini (server.ini.example 참고)
    QSettings config("server.ini", QSettings::IniFormat);

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
