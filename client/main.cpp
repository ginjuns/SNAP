#include <QApplication>
#include <QLocale>
#include <QMessageBox>
#include <QStringList>
#include <QTextCodec>

#include "embeddedserver.h"
#include "mainwindow.h"
#include "serverclient.h"

// 사용법: KioskClient [--host 서버IP] [--fullscreen]
// --host 를 주지 않으면 키오스크 안에서 서버(9000번 포트)도 함께 실행한다.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#if QT_VERSION < 0x050000
    // 소스 코드의 한글 문자열(UTF-8)을 그대로 사용하기 위한 설정 (Qt5는 기본이 UTF-8)
    QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForCStrings(utf8);
    QTextCodec::setCodecForTr(utf8);
#endif
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));

    app.setStyleSheet("QWidget { font-size: 16px; }"
                      "QPushButton { min-height: 40px; padding: 4px 16px; }");

    const QStringList args = app.arguments();
    const int hostIndex = args.indexOf("--host");
    EmbeddedServer server;
    if (hostIndex >= 0 && hostIndex + 1 < args.size()) {
        ServerClient::setHost(args.at(hostIndex + 1));
    } else {
        QString err;
        if (!server.startAndWait(&err))
            QMessageBox::warning(0, "서버", "내장 서버를 시작하지 못했습니다.\n" + err);
    }

    MainWindow window;
    if (args.contains("--fullscreen"))
        window.showFullScreen();
    else
        window.show();
    return app.exec();
}
