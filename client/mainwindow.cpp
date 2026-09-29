#include "mainwindow.h"
#include "adminwidget.h"
#include "facedialog.h"
#include "memberwidget.h"
#include "serverclient.h"

#include "qtcompat.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("무인 키오스크");
    resize(1024, 768);

    // 초기 화면
    m_login = new QWidget;
    QLabel *title = new QLabel("무인 키오스크");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 40px; font-weight: bold;");
    QLabel *guide = new QLabel("아래 버튼을 누르고 카메라를 바라봐 주세요.");
    guide->setAlignment(Qt::AlignCenter);
    QPushButton *loginButton = new QPushButton("얼굴인식으로 시작하기");
    loginButton->setMinimumSize(360, 100);
    loginButton->setStyleSheet("font-size: 24px;");
    connect(loginButton, SIGNAL(clicked()), SLOT(faceLogin()));

    QVBoxLayout *v = new QVBoxLayout(m_login);
    v->addStretch();
    v->addWidget(title);
    v->addWidget(guide);
    v->addSpacing(40);
    v->addWidget(loginButton, 0, Qt::AlignCenter);
    v->addStretch();

    m_member = new MemberWidget;
    m_admin = new AdminWidget;
    connect(m_member, SIGNAL(finished()), SLOT(showLogin()));
    connect(m_admin, SIGNAL(finished()), SLOT(showLogin()));

    m_stack = new QStackedWidget;
    m_stack->addWidget(m_login);
    m_stack->addWidget(m_member);
    m_stack->addWidget(m_admin);
    setCentralWidget(m_stack);
}

void MainWindow::faceLogin()
{
    FaceDialog face("얼굴인식 로그인", this);
    if (face.exec() != QDialog::Accepted)
        return;

    QVariant data;
    QString err;
    if (!ServerClient::call("LOGIN", QVariantMap{{"userId", face.userId()}}, &data, &err)) {
        QMessageBox::warning(this, "로그인 실패", err);
        return;
    }

    const QVariantMap user = data.toMap();
    if (user.value("role").toString() == "admin") {
        m_admin->start(user);
        m_stack->setCurrentWidget(m_admin);
    } else {
        m_member->start(user);
        m_stack->setCurrentWidget(m_member);
    }
}

void MainWindow::showLogin()
{
    m_stack->setCurrentWidget(m_login);
}
