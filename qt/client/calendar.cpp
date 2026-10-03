#include "calendar.h"
#include "ui_calendar.h"
#include "util.h"

Calendar::Calendar(const QDate &date, bool month, QWidget *parent)
    : QDialog(parent), ui(new Ui::Calendar), m_month(month)
{
    ui->setupUi(this);
    scaleUi(this);
    ui->cal->setSelectedDate(date);

    connect(ui->btnPrevYear, SIGNAL(clicked()), ui->cal, SLOT(showPreviousYear()));
    connect(ui->btnNextYear, SIGNAL(clicked()), ui->cal, SLOT(showNextYear()));
    connect(ui->btnPrevMonth, SIGNAL(clicked()), ui->cal, SLOT(showPreviousMonth()));
    connect(ui->btnNextMonth, SIGNAL(clicked()), ui->cal, SLOT(showNextMonth()));
    connect(ui->cal, SIGNAL(currentPageChanged(int,int)), SLOT(updateTitle()));
    connect(ui->btnCancel, SIGNAL(clicked()), SLOT(reject()));
    connect(ui->btnOk, SIGNAL(clicked()), SLOT(accept()));
    if (!month)
        connect(ui->cal, SIGNAL(clicked(QDate)), SLOT(accept()));

    updateTitle();
}

Calendar::~Calendar()
{
    delete ui;
}

QDate Calendar::date() const
{
    if (m_month)
        return QDate(ui->cal->yearShown(), ui->cal->monthShown(), 1);
    return ui->cal->selectedDate();
}

void Calendar::updateTitle()
{
    ui->lblYear->setText(QString("%1년").arg(ui->cal->yearShown()));
    ui->lblMonth->setText(QString("%1월").arg(ui->cal->monthShown()));
}
