#include "passworddialog.h"
#include "ui_passworddialog.h"
#include "serverclient.h"
#include "virtualkeyboard.h"

#include "kioskdialog.h"
#include "qtcompat.h"

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::PasswordDialog), m_userId(-1)
{
    ui->setupUi(this);   // 화면 배치: passworddialog.ui
    scaleUi(this);
    resize(px(480), sizeHint().height());

    VirtualKeyboard::attach(ui->loginIdEdit, VirtualKeyboard::English, "아이디");
    VirtualKeyboard::attach(ui->passwordEdit, VirtualKeyboard::English, "비밀번호");
    connect(ui->cancelButton, SIGNAL(clicked()), SLOT(reject()));
    connect(ui->okButton, SIGNAL(clicked()), SLOT(login()));

    QTimer::singleShot(0, this, SLOT(startInput()));
}

PasswordDialog::~PasswordDialog()
{
    delete ui;
}

void PasswordDialog::startInput()
{
    if (VirtualKeyboard::open(ui->loginIdEdit, VirtualKeyboard::English, "아이디") && !ui->loginIdEdit->text().isEmpty()
        && VirtualKeyboard::open(ui->passwordEdit, VirtualKeyboard::English, "비밀번호"))
        login();
}

void PasswordDialog::login()
{
    if (ui->loginIdEdit->text().isEmpty() || ui->passwordEdit->text().isEmpty()) {
        KioskMessage::information(this, windowTitle(), "아이디와 비밀번호를 입력해 주세요.");
        return;
    }

    QVariant data;
    QString err;
    const QVariantMap args{{"loginId", ui->loginIdEdit->text()}, {"password", ui->passwordEdit->text()}};
    if (!ServerClient::call("PASSWORD_LOGIN", args, &data, &err)) {
        KioskMessage::warning(this, "로그인 실패", err);
        ui->passwordEdit->clear();
        return;
    }
    m_userId = data.toMap().value("id").toInt();
    accept();
}
