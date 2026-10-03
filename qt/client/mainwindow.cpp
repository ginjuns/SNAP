#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adminwidget.h"
#include "facedialog.h"
#include "memberwidget.h"
#include "serverclient.h"

#include "kioskdialog.h"
#include "qtcompat.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);   // 초기 화면: mainwindow.ui
    scaleUi(this);
    resize(px(720), px(1280));   // 세로 화면 기준 (main.cpp의 배율로 실제 크기가 정해짐)
    connect(ui->loginButton, SIGNAL(clicked()), SLOT(faceLogin()));

    // 일반회원 / 관리자 화면은 초기 화면 뒤에 붙인다
    m_member = new MemberWidget;
    m_admin = new AdminWidget;
    connect(m_member, SIGNAL(finished()), SLOT(showLogin()));
    connect(m_admin, SIGNAL(finished()), SLOT(showLogin()));
    ui->stack->addWidget(m_member);
    ui->stack->addWidget(m_admin);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::faceLogin()
{
    FaceDialog face("얼굴인식 로그인", this, true);
    if (execInWindow(&face, true) != QDialog::Accepted)
        return;

    QVariant data;
    QString err;
    if (!ServerClient::call("LOGIN", QVariantMap{{"userId", face.userId()}}, &data, &err)) {
        KioskMessage::warning(this, "로그인 실패", err);
        return;
    }

    const QVariantMap user = data.toMap();
    if (user.value("role").toString() == "admin") {
        m_admin->start(user);
        ui->stack->setCurrentWidget(m_admin);
    } else {
        m_member->start(user);
        ui->stack->setCurrentWidget(m_member);
    }
}

void MainWindow::showLogin()
{
    ui->stack->setCurrentWidget(ui->loginPage);
}
