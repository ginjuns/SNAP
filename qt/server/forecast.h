// 판매 예측 계산: 예측 판매량, 품절 예상일, 발주 수량 (forecast.cpp 참고)

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
