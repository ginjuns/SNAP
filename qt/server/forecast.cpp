#include "forecast.h"
#include <cmath>

Forecast predict(const QVector<int> &daily, int firstWeekday, int days)
{
    Forecast f;
    int n = daily.size();
    if (n == 0) {
        f.qty.fill(0, days);
        f.low = f.qty;
        f.high = f.qty;
        return f;
    }
    double sum[7] = {0}, total = 0;
    int cnt[7] = {0};
    for (int i = 0; i < n; ++i) {
        int w = (firstWeekday - 1 + i) % 7;
        sum[w] += daily[i];
        cnt[w]++;
        total += daily[i];
    }
    double mean = total / n;
    double factor[7];
    for (int w = 0; w < 7; ++w)
        factor[w] = (mean > 0 && cnt[w]) ? sum[w] / cnt[w] / mean : 1.0;
    double sx = 0, sy = 0, sxy = 0, sxx = 0;
    int m = 0;
    for (int i = 0; i < n; ++i) {
        int w = (firstWeekday - 1 + i) % 7;
        if (factor[w] <= 0)
            continue;
        double y = daily[i] / factor[w];
        sx += i;
        sy += y;
        sxy += i * y;
        sxx += i * i;
        ++m;
    }
    double den = m * sxx - sx * sx;
    double b = den != 0 ? (m * sxy - sx * sy) / den : 0;
    double a = m ? (sy - b * sx) / m : 0;
    double se = 0;
    for (int i = 0; i < n; ++i) {
        int w = (firstWeekday - 1 + i) % 7;
        double d = daily[i] - (a + b * i) * factor[w];
        se += d * d;
    }
    double sigma = std::sqrt(se / n);
    for (int d = 0; d < days; ++d) {
        int x = n + d;
        int w = (firstWeekday - 1 + x) % 7;
        double v = qMax(0.0, (a + b * x) * factor[w]);
        f.qty << v;
        f.low << qMax(0.0, v - 1.28 * sigma);
        f.high << v + 1.28 * sigma;
    }
    return f;
}
int daysLeft(int stock, const Forecast &f)
{
    if (stock <= 0)
        return 0;
    double sold = 0;
    for (int d = 0; d < f.qty.size(); ++d) {
        sold += f.qty[d];
        if (sold >= stock)
            return d + 1;
    }
    return -1;
}
int orderQty(int stock, const Forecast &f, int leadDays)
{
    double need = 0;
    for (int d = 0; d < leadDays && d < f.qty.size(); ++d)
        need += f.qty[d];
    return qMax(0, (int)std::ceil(need * 1.2) - stock);
}
