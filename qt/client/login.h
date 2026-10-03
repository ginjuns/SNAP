#ifndef LOGIN_H
#define LOGIN_H

#include <QDialog>

namespace Ui {
class Login;
}

class Login : public QDialog
{
    Q_OBJECT
public:
    explicit Login(QWidget *parent = 0);
    ~Login();

    int id() const { return m_id; }

private slots:
    void input();
    void login();

private:
    Ui::Login *ui;
    int m_id;
};

#endif
