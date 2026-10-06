#include "forecast.h"

Forecast predict(const QVector<int> &daily, int firstWeekday, int days)
{
    // TODO: 추세(이동평균/선형회귀) x 요일 계수
    Q_UNUSED(daily);
    Q_UNUSED(firstWeekday);
    Forecast f;
    f.qty.fill(0, days);
    f.low.fill(0, days);
    f.high.fill(0, days);
    return f;
}

int daysLeft(int stock, const Forecast &f)
{
    // TODO: 예측 판매량을 하루씩 누적해 stock을 넘는 날
    Q_UNUSED(stock);
    Q_UNUSED(f);
    return -1;
}

int orderQty(int stock, const Forecast &f, int leadDays)
{
    // TODO: leadDays 동안의 예측 합계(+여유분) - stock
    Q_UNUSED(stock);
    Q_UNUSED(f);
    Q_UNUSED(leadDays);
    return 0;
}
