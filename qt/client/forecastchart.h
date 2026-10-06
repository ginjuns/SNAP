#ifndef FORECASTCHART_H
#define FORECASTCHART_H

#include <QWidget>
#include <QVariant>

class ForecastChart : public QWidget
{
    Q_OBJECT
public:
    explicit ForecastChart(QWidget *parent = 0);

    void setData(const QVariantList &past, const QVariantList &future, const QString &title);

protected:
    void paintEvent(QPaintEvent *e);

private:
    QVariantList m_past;
    QVariantList m_future;
    QString m_title;
};

#endif
