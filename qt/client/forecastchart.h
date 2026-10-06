#ifndef FORECASTCHART_H
#define FORECASTCHART_H

#include <QWidget>
#include <QVariant>

// 실제 판매량(막대) + AI 예측 판매량(점선)을 함께 그리는 차트
class ForecastChart : public QWidget
{
    Q_OBJECT
public:
    explicit ForecastChart(QWidget *parent = 0);

    // past: 지난 판매량 [{label, qty}], future: 예측 [{label, qty, low, high}]
    void setData(const QVariantList &past, const QVariantList &future, const QString &title);

protected:
    void paintEvent(QPaintEvent *e);

private:
    QVariantList m_past;
    QVariantList m_future;
    QString m_title;
};

#endif
