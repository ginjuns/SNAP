#include <QCoreApplication>
#include <QTextCodec>

#include "database.h"
#include "kioskserver.h"
#include "protocol.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextCodec::setCodecForCStrings(QTextCodec::codecForName("UTF-8"));

    Database db;
    QString err;
    if (!db.open("kiosk.db", &err)) {
        qCritical("DB 연결 실패: %s", qPrintable(err));
        return 1;
    }

    KioskServer server(&db);
    if (!server.listen(QHostAddress::Any, KIOSK_PORT)) {
        qCritical("포트 %d 열기 실패: %s", KIOSK_PORT, qPrintable(server.errorString()));
        return 1;
    }
    qDebug("키오스크 DB 서버 시작 (포트 %d)", KIOSK_PORT);
    return app.exec();
}
