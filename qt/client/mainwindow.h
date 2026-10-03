#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVariant>

class MemberWidget;
class AdminWidget;

namespace Ui {
class MainWindow;
}

// 화면 전환 담당: 초기(로그인) 화면 -> 일반회원 화면 / 관리자 화면
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void faceLogin();
    void showLogin();

private:
    Ui::MainWindow *ui;
    MemberWidget *m_member;
    AdminWidget *m_admin;
};

#endif // MAINWINDOW_H
