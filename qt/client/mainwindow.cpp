#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "admin.h"
#include "face.h"
#include "member.h"
#include "net.h"
#include "popup.h"
#include "util.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    scaleUi(this);
    resize(px(720), px(1280));
    connect(ui->btnLogin, SIGNAL(clicked()), SLOT(login()));

    m_member = new Member;
    m_admin = new Admin;
    connect(m_member, SIGNAL(done()), SLOT(home()));
    connect(m_admin, SIGNAL(done()), SLOT(home()));
    ui->stack->addWidget(m_member);
    ui->stack->addWidget(m_admin);
}

MainWindow::~MainWindow()
{
    delete ui;
}

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

    QVariantMap user = data.toMap();
    if (user.value("role").toString() == "admin") {
        m_admin->start(user);
        ui->stack->setCurrentWidget(m_admin);
    } else {
        m_member->start(user);
        ui->stack->setCurrentWidget(m_member);
    }
}

void MainWindow::home()
{
    ui->stack->setCurrentWidget(ui->pageLogin);
}
