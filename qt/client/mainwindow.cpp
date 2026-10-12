#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "admin.h"
#include "server.h"
#include "face.h"
#include "member.h"
#include "net.h"
#include "popup.h"
#include "util.h"

// Wayland 세션인지 확인 (모니터 끄는 방법이 X11 과 다름)
static bool wayland()
{
    return qgetenv("XDG_SESSION_TYPE") == "wayland";
}

// 모니터 켜기/끄기 (X11: xset, Wayland: Mutter DBus)
static void screenPower(bool on)
{
    if (wayland()) {
        QProcess::execute("busctl", QStringList() << "--user" << "set-property"
                                                  << "org.gnome.Mutter.DisplayConfig"
                                                  << "/org/gnome/Mutter/DisplayConfig"
                                                  << "org.gnome.Mutter.DisplayConfig"
                                                  << "PowerSaveMode" << "i" << (on ? "0" : "3"));
    } else {
        QProcess::execute("xset", QStringList() << "dpms" << "force" << (on ? "on" : "off"));
    }
}

// 화면 구성: 회원/관리자 화면을 스택에 넣고, 화면보호기 끄기, ESP32 사람 감지 연결 후 절전 상태로 시작
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_sleep(false)
{
    ui->setupUi(this);
    scaleUi(this);
    resize(px(1280), px(720));
    ui->lblTitle->setPixmap(QPixmap(":/images/snap_logo.png")
                                .scaled(px(260), px(260), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    connect(ui->btnLogin, SIGNAL(clicked()), SLOT(login()));

    m_member = new Member;
    m_admin = new Admin;
    connect(m_member, SIGNAL(done()), SLOT(home()));
    connect(m_admin, SIGNAL(done()), SLOT(home()));
    ui->stack->addWidget(m_member);
    ui->stack->addWidget(m_admin);

    if (!wayland()) {
        QProcess::execute("xset", QStringList() << "s" << "off");
        QProcess::execute("xset", QStringList() << "dpms" << "0" << "0" << "0");
    }

    ui->pageSleep->installEventFilter(this);
    if (Esp::get())
        connect(Esp::get(), SIGNAL(presence(bool)), SLOT(onPresence(bool)));
    sleep();
}

// 종료할 때 모니터를 다시 켜 둠
MainWindow::~MainWindow()
{
    screenPower(true);
    delete ui;
}

// 얼굴인식 또는 비밀번호로 로그인한 뒤 권한에 따라 관리자/회원 화면으로 이동
void MainWindow::login()
{
    Face face("얼굴인식 로그인", this, true);
    if (popup(&face, true) != QDialog::Accepted)
        return;

    QVariant data;
    QString err;
    if (!Net::call("LOGIN", QVariantMap{{"userId", face.id()}}, &data, &err)) {
        Msg::warn(this, "로그인 실패", err);
        return;
    }
    if (m_sleep)
        return;

    QVariantMap user = data.toMap();
    user["pwLogin"] = face.byPassword();
    if (user.value("role").toString() == "admin") {
        m_admin->start(user);
        ui->stack->setCurrentWidget(m_admin);
    } else {
        m_member->start(user);
        ui->stack->setCurrentWidget(m_member);
    }
}

// 로그아웃: 첫 화면으로 돌아감 (절전 중이면 그대로)
void MainWindow::home()
{
    if (!m_sleep)
        ui->stack->setCurrentWidget(ui->pageLogin);
}

// ESP32 사람 감지 신호: 사람이 오면 깨우고, 떠나면 절전
void MainWindow::onPresence(bool on)
{
    if (on)
        wake();
    else
        sleep();
}

// 절전 모드: 열린 팝업을 모두 닫고 절전 화면으로 바꾼 뒤 모니터 끄기
void MainWindow::sleep()
{
    m_sleep = true;
    setBlocked(true);
    closeAll();
    ui->stack->setCurrentWidget(ui->pageSleep);
    screenPower(false);
}

// 절전 해제: 첫 화면으로 돌아가고 모니터 켜기
void MainWindow::wake()
{
    if (!m_sleep)
        return;
    m_sleep = false;
    setBlocked(false);
    ui->stack->setCurrentWidget(ui->pageLogin);
    screenPower(true);
}

// 절전 화면을 터치하면 깨우기
bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == ui->pageSleep && e->type() == QEvent::MouseButtonRelease) {
        wake();
        return true;
    }
    return QMainWindow::eventFilter(obj, e);
}
