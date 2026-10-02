#include <QApplication>
#include <QLocale>
#include <QMessageBox>
#include <QScreen>
#include <QStringList>
#include <QTextCodec>

#include "embeddedserver.h"
#include "mainwindow.h"
#include "serverclient.h"
#include "uiscale.h"

// 사용법: KioskClient [--host 서버IP] [--scale 배율]
// --host 를 주지 않으면 키오스크 안에서 서버(9000번 포트)도 함께 실행한다.
//
// 항상 전체 화면으로 실행된다. (종료: Alt+F4)
// 화면은 세로 720x1280 기준 크기로 만들고, 모니터에 맞춰 글자/버튼 크기에 배율을 곱한다. (uiscale.h)
//   15.6인치 세로(1080x1920) -> 1.5배
// --scale 배율 : 자동 대신 직접 지정
int main(int argc, char *argv[])
{
    qunsetenv("QT_SCALE_FACTOR");   // Qt 자체 배율은 쓰지 않는다 (일부 환경에서 화면이 하얗게만 나옴)
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    const QSize screen = app.primaryScreen()->size();
    uiScale() = qMin(screen.width() / 720.0, screen.height() / 1280.0);
    const int scaleIndex = args.indexOf("--scale");
    if (scaleIndex >= 0 && scaleIndex + 1 < args.size() && args.at(scaleIndex + 1).toDouble() > 0)
        uiScale() = args.at(scaleIndex + 1).toDouble();

#if QT_VERSION < 0x050000
    // 소스 코드의 한글 문자열(UTF-8)을 그대로 사용하기 위한 설정 (Qt5는 기본이 UTF-8)
    QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForCStrings(utf8);
    QTextCodec::setCodecForTr(utf8);
#endif
    QLocale::setDefault(QLocale(QLocale::Korean, QLocale::SouthKorea));

    app.setStyleSheet(css("QWidget { font-size: 16px; }"
                          "QPushButton { min-height: 40px; padding: 4px 16px; }"));

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
    window.showFullScreen();
    return app.exec();
}
