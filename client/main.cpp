#include <QApplication>
#include <QLocale>
#include <QMessageBox>
#include <QScreen>
#include <QStringList>
#include <QTextCodec>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#include "embeddedserver.h"
#include "mainwindow.h"
#include "serverclient.h"

// 사용법: KioskClient [--host 서버IP] [--scale 배율]
// --host 를 주지 않으면 키오스크 안에서 서버(9000번 포트)도 함께 실행한다.
//
// 항상 전체 화면으로 실행된다. (종료: Alt+F4)
// 화면은 세로 720x1280 크기로 배치하고, 모니터에 꽉 차도록 배율을 곱해 그린다.
//   15.6인치 세로(1080x1920) -> 1.5배
// 배율은 QApplication 을 만들기 전에만 정할 수 있어서,
// 모니터 크기를 확인한 뒤 배율(QT_SCALE_FACTOR)을 넣고 프로그램을 다시 실행한다.
// --scale 배율 : 자동 대신 직접 지정
int main(int argc, char *argv[])
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (qstrcmp(argv[i], "--scale") == 0)
            qputenv("QT_SCALE_FACTOR", argv[i + 1]);
    }

    QApplication app(argc, argv);

#ifdef Q_OS_UNIX
    if (!qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        const QSize screen = app.primaryScreen()->size();
        const double scale = qMin(screen.width() / 720.0, screen.height() / 1280.0);
        qputenv("QT_SCALE_FACTOR", QByteArray::number(scale, 'f', 2));
        execv("/proc/self/exe", argv);   // 성공하면 여기로 돌아오지 않는다. 실패하면 배율 없이 계속
    }
#endif

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
    window.showFullScreen();
    return app.exec();
}
