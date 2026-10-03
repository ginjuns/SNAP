#include "esp.h"
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
                          "QPushButton { min-height: 40px; padding: 4px 16px; }"
                          "QScrollBar:vertical { width: 32px; background: #f0efea; margin: 0; }"
                          "QScrollBar:horizontal { height: 32px; background: #f0efea; margin: 0; }"
                          "QScrollBar::handle:vertical { background: #a8a69e; min-height: 60px; border-radius: 8px; margin: 4px; }"
                          "QScrollBar::handle:horizontal { background: #a8a69e; min-width: 60px; border-radius: 8px; margin: 4px; }"
                          "QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }"
                          "QScrollBar::add-page, QScrollBar::sub-page { background: none; }"));

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
