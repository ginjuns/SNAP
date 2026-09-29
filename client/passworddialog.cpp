#include "passworddialog.h"
#include "serverclient.h"
#include "virtualkeyboard.h"

#include "qtcompat.h"

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent), m_userId(-1)
{
    setWindowTitle("비밀번호로 로그인");

    m_loginId = new QLineEdit;
    m_loginId->setPlaceholderText("아이디");
    m_password = new QLineEdit;
    m_password->setPlaceholderText("비밀번호");
    m_password->setEchoMode(QLineEdit::Password);
    VirtualKeyboard::attach(m_loginId, VirtualKeyboard::English, "아이디");
    VirtualKeyboard::attach(m_password, VirtualKeyboard::English, "비밀번호");

    QFormLayout *form = new QFormLayout;
    form->addRow("아이디", m_loginId);
    form->addRow("비밀번호", m_password);

    QPushButton *cancel = new QPushButton("취소");
    QPushButton *ok = new QPushButton("로그인");
    ok->setStyleSheet("background: #2a78d6; color: white; font-weight: bold;");
    connect(cancel, SIGNAL(clicked()), SLOT(reject()));
    connect(ok, SIGNAL(clicked()), SLOT(login()));
    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(cancel);
    buttons->addWidget(ok);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttons);
    resize(480, sizeHint().height());

    QTimer::singleShot(0, this, SLOT(startInput()));
}

void PasswordDialog::startInput()
{
    if (VirtualKeyboard::open(m_loginId, VirtualKeyboard::English, "아이디") && !m_loginId->text().isEmpty()
        && VirtualKeyboard::open(m_password, VirtualKeyboard::English, "비밀번호"))
        login();
}

void PasswordDialog::login()
{
    if (m_loginId->text().isEmpty() || m_password->text().isEmpty()) {
        QMessageBox::information(this, windowTitle(), "아이디와 비밀번호를 입력해 주세요.");
        return;
    }

    QVariant data;
    QString err;
    const QVariantMap args{{"loginId", m_loginId->text()}, {"password", m_password->text()}};
    if (!ServerClient::call("PASSWORD_LOGIN", args, &data, &err)) {
        QMessageBox::warning(this, "로그인 실패", err);
        m_password->clear();
        return;
    }
    m_userId = data.toMap().value("id").toInt();
    accept();
}
