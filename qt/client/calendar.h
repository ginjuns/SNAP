#ifndef CALENDAR_H
#define CALENDAR_H

#include <QDate>
#include <QDialog>

namespace Ui {
class Calendar;
}

class Calendar : public QDialog
{
    Q_OBJECT
public:
    Calendar(const QDate &date, bool month, QWidget *parent = 0);
    ~Calendar();

    QDate date() const;

private slots:
    void updateTitle();

private:
    Ui::Calendar *ui;
    bool m_month;
};

#endif
