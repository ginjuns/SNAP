#include <QApplication>
#include <QLocale>
#include <QStringList>
#include <QTextCodec>

#include "mainwindow.h"
#include "serverclient.h"

// 사용법: KioskClient [--host 서버IP] [--fullscreen]
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 소스 코드의 한글 문자열(UTF-8)을 그대로 사용하기 위한 설정 (Qt4 전용)
    QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForCStrings(utf8);
    QTextCodec::setCodecForTr(utf8);
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));

    app.setStyleSheet("QWidget { font-size: 16px; }"
                      "QPushButton { min-height: 40px; padding: 4px 16px; }");

    const QStringList args = app.arguments();
    const int hostIndex = args.indexOf("--host");
    if (hostIndex >= 0 && hostIndex + 1 < args.size())
        ServerClient::setHost(args.at(hostIndex + 1));

    MainWindow window;
    if (args.contains("--fullscreen"))
        window.showFullScreen();
    else
        window.show();
    return app.exec();
}
