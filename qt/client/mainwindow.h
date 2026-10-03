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

protected:
    bool eventFilter(QObject *obj, QEvent *e);

private slots:
    void login();
    void home();
    void onPresence(bool on);

private:
    void sleep();
    void wake();

    Ui::MainWindow *ui;
    Member *m_member;
    Admin *m_admin;
    bool m_sleep;
};

#endif
