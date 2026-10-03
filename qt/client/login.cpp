#include "login.h"
#include "ui_login.h"
#include "keyboard.h"
#include "net.h"
#include "popup.h"
#include "util.h"

Login::Login(QWidget *parent)
    : QDialog(parent), ui(new Ui::Login), m_id(-1)
{
    ui->setupUi(this);
    scaleUi(this);
    resize(px(480), sizeHint().height());

    Keyboard::attach(ui->editId, Keyboard::English, "아이디");
    Keyboard::attach(ui->editPw, Keyboard::English, "비밀번호");
    connect(ui->btnCancel, SIGNAL(clicked()), SLOT(reject()));
    connect(ui->btnOk, SIGNAL(clicked()), SLOT(login()));

    QTimer::singleShot(0, this, SLOT(input()));
}

Login::~Login()
{
    delete ui;
}

void Login::input()
{
    if (Keyboard::open(ui->editId, Keyboard::English, "아이디") && !ui->editId->text().isEmpty()
        && Keyboard::open(ui->editPw, Keyboard::English, "비밀번호"))
        login();
}

void Login::login()
{
    if (ui->editId->text().isEmpty() || ui->editPw->text().isEmpty()) {
        Msg::info(this, windowTitle(), "아이디와 비밀번호를 입력해 주세요.");
        return;
    }

    QVariant data;
    QString err;
    QVariantMap args{{"loginId", ui->editId->text()}, {"password", ui->editPw->text()}};
    if (!Net::call("PASSWORD_LOGIN", args, &data, &err)) {
        Msg::warn(this, "로그인 실패", err);
        ui->editPw->clear();
        return;
    }
    m_id = data.toMap().value("id").toInt();
    accept();
}
