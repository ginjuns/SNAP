#ifndef FORECAST_H
#define FORECAST_H

#include <QVector>

struct Forecast
{
    QVector<double> qty;
    QVector<double> low;
    QVector<double> high;
};

Forecast predict(const QVector<int> &daily, int firstWeekday, int days);

int daysLeft(int stock, const Forecast &f);
int orderQty(int stock, const Forecast &f, int leadDays);

#endif
