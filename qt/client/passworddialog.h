#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>

class QLineEdit;

// 얼굴인식이 안 될 때 쓰는 아이디/비밀번호 로그인 창.
// 창이 뜨면 가상 키보드로 아이디 -> 비밀번호 순서로 입력받고, 성공 시 accept(), userId()로 결과 확인.
class PasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PasswordDialog(QWidget *parent = 0);

    int userId() const { return m_userId; }

private slots:
    void startInput();   // 창이 뜨자마자 아이디 키보드부터 띄운다
    void login();

private:
    QLineEdit *m_loginId;
    QLineEdit *m_password;
    int m_userId;
};

#endif // PASSWORDDIALOG_H
