#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVariant>

class QStackedWidget;
class QWidget;
class MemberWidget;
class AdminWidget;

// 화면 전환 담당: 초기(로그인) 화면 -> 일반회원 화면 / 관리자 화면
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);

private slots:
    void faceLogin();
    void showLogin();

private:
    QStackedWidget *m_stack;
    QWidget *m_login;
    MemberWidget *m_member;
    AdminWidget *m_admin;
};

#endif // MAINWINDOW_H
