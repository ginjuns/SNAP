#ifndef FORECAST_H
#define FORECAST_H

#include <QVector>

// 일별 판매량으로 앞으로 days일의 판매량을 예측한다.
struct Forecast
{
    QVector<double> qty;     // 예측 판매량 (하루 단위)
    QVector<double> low;     // 예측 하한
    QVector<double> high;    // 예측 상한
};

Forecast predict(const QVector<int> &daily, int firstWeekday, int days);

// 재고와 예측으로 품절까지 남은 일수(-1 = 기간 안에 품절 안 됨)와 추천 발주량을 구한다.
int daysLeft(int stock, const Forecast &f);
int orderQty(int stock, const Forecast &f, int leadDays);

#endif
