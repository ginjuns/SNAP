#include "forecastchart.h"
#include "util.h"

ForecastChart::ForecastChart(QWidget *parent)
    : QWidget(parent)
{
}

void ForecastChart::setData(const QVariantList &past, const QVariantList &future, const QString &title)
{
    m_past = past;
    m_future = future;
    m_title = title;
    update();
}

void ForecastChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#fcfcfb"));
    // TODO: 축/눈금 -> 지난 판매량 막대 -> 예측 점선 + 오차 범위(low~high) 음영 -> "오늘" 기준선
    p.setPen(QColor("#898781"));
    p.drawText(rect(), Qt::AlignCenter, m_title.isEmpty() ? "[예측하기]를 누르세요" : m_title);
}
