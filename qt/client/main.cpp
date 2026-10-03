#include "mainwindow.h"
#include "net.h"
#include "serverthread.h"
#include "util.h"

int main(int argc, char *argv[])
{
    qunsetenv("QT_SCALE_FACTOR");
    QApplication app(argc, argv);

    QStringList args = app.arguments();
    QSize size = app.primaryScreen()->size();
    uiScale() = qMin(size.width() / 720.0, size.height() / 1280.0);
    int i = args.indexOf("--scale");
    if (i >= 0 && i + 1 < args.size() && args.at(i + 1).toDouble() > 0)
        uiScale() = args.at(i + 1).toDouble();

    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));

    app.setStyleSheet(css("QWidget { font-size: 16px; }"
                          "QPushButton { min-height: 40px; padding: 4px 16px; }"));

    ServerThread server;
    i = args.indexOf("--host");
    if (i >= 0 && i + 1 < args.size()) {
        Net::setHost(args.at(i + 1));
    } else {
        QString err;
        if (!server.startWait(&err))
            QMessageBox::warning(0, "서버", "내장 서버를 시작하지 못했습니다.\n" + err);
    }

    MainWindow w;
    w.showFullScreen();
    return app.exec();
}
