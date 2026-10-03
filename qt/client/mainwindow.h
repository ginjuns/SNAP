#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class Member;
class Admin;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void login();
    void home();

private:
    Ui::MainWindow *ui;
    Member *m_member;
    Admin *m_admin;
};

#endif
