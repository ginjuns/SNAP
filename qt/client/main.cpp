#include "server.h"
#include "mainwindow.h"
#include "net.h"
#include "util.h"

// 프로그램 시작: 화면 배율/스타일 설정, 내장 서버·ESP32 서버 실행 후 메인 화면 표시
int main(int argc, char *argv[])
{
    qunsetenv("QT_SCALE_FACTOR");
    QApplication app(argc, argv);

    QStringList args = app.arguments();
    QSize size = app.primaryScreen()->size();
    uiScale() = qMin(size.width() / 1280.0, size.height() / 720.0);   // 가로 1280x720 기준
    int i = args.indexOf("--scale");
    if (i >= 0 && i + 1 < args.size() && args.at(i + 1).toDouble() > 0)
        uiScale() = args.at(i + 1).toDouble();

    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));

    app.setStyleSheet(appStyle());
    app.setWindowIcon(QIcon(":/images/snap_logo.png"));


    ServerThread server;
    i = args.indexOf("--host");
    if (i >= 0 && i + 1 < args.size()) {
        Net::setHost(args.at(i + 1));
    } else {
        QString err;
        if (!server.startWait(&err))
            QMessageBox::warning(0, "서버", "내장 서버를 시작하지 못했습니다.\n" + err);
    }

    Esp esp;
    QString espErr;
    if (!esp.start(&espErr))
        QMessageBox::warning(0, "ESP32", espErr);

    MainWindow w;
    w.showFullScreen();
    return app.exec();
}
