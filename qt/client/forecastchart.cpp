#include "forecastchart.h"
#include "util.h"

ForecastChart::ForecastChart(QWidget *parent)
    : QWidget(parent)
{
}
// 지난 판매량, 예측 판매량, 제목을 받아 다시 그림
void ForecastChart::setData(const QVariantList &past, const QVariantList &future, const QString &title)
{
    m_past = past;
    m_future = future;
    m_title = title;
    update();
}
// 지난 판매량은 막대, 예측은 선과 예측 범위 띠로 그림
void ForecastChart::paintEvent(QPaintEvent *)
{
    const QColor INK("#0b0b0b"), GRAY("#898781"), GRID("#e1e0d9"), BAR(Theme::GREEN), PRED("#eb6834");
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#fcfcfb"));
    int n1 = m_past.size();
    int n = n1 + m_future.size();
    if (n == 0) {
        p.setPen(GRAY);
        p.drawText(rect(), Qt::AlignCenter, "[예측하기]를 누르세요");
        return;
    }
    QRectF area(px(48), px(40), width() - px(64), height() - px(80));
    double max = 1;
    foreach (const QVariant &v, m_past)
        max = qMax(max, v.toMap().value("qty").toDouble());
    foreach (const QVariant &v, m_future)
        max = qMax(max, v.toMap().value("high").toDouble());
    max *= 1.1;
    double slot = area.width() / n;
    auto X = [&](int i) { return area.left() + slot * (i + 0.5); };
    auto Y = [&](double v) { return area.bottom() - v / max * area.height(); };
    QFont bold = font();
    bold.setBold(true);
    p.setFont(bold);
    p.setPen(INK);
    p.drawText(QRectF(0, 0, width(), px(36)), Qt::AlignCenter, m_title);
    p.setFont(font());
    for (int k = 0; k <= 4; ++k) {
        double v = max * k / 4;
        p.setPen(GRID);
        p.drawLine(QPointF(area.left(), Y(v)), QPointF(area.right(), Y(v)));
        p.setPen(GRAY);
        p.drawText(QRectF(0, Y(v) - px(10), area.left() - px(6), px(20)),Qt::AlignRight | Qt::AlignVCenter, QString::number(qRound(v)));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(BAR);
    for (int i = 0; i < n1; ++i) {
        double q = m_past[i].toMap().value("qty").toDouble();
        p.drawRect(QRectF(X(i) - slot * 0.35, Y(q), slot * 0.7, area.bottom() - Y(q)));
    }
    QPainterPath band;
    for (int j = 0; j < m_future.size(); ++j) {
        QPointF pt(X(n1 + j), Y(m_future[j].toMap().value("high").toDouble()));
        if (j == 0)
            band.moveTo(pt);
        else
            band.lineTo(pt);
    }
    for (int j = m_future.size() - 1; j >= 0; --j)
        band.lineTo(X(n1 + j), Y(m_future[j].toMap().value("low").toDouble()));
    band.closeSubpath();
    p.setBrush(QColor(PRED.red(), PRED.green(), PRED.blue(), 50));
    p.drawPath(band);
    QPolygonF line;
    for (int j = 0; j < m_future.size(); ++j)
        line << QPointF(X(n1 + j), Y(m_future[j].toMap().value("qty").toDouble()));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(PRED, px(3), Qt::DashLine));
    p.drawPolyline(line);
    double today = area.left() + slot * n1;
    p.setPen(QPen(GRAY, 1, Qt::DotLine));
    p.drawLine(QPointF(today, area.top()), QPointF(today, area.bottom()));
    p.drawText(QPointF(today + px(4), area.top() + px(14)), "오늘");
    p.setPen(GRAY);
    // 날짜는 오늘 칸을 기준으로 step 간격마다 표시 (오늘·마지막 날이 항상 보이게)
    int step = qMax(1, n / 7);
    int last = n - 1;
    int start = (n1 < n ? n1 : last) % step;
    QList<int> marks;
    for (int i = start; i < n; i += step)
        marks << i;
    if (marks.last() != last) {
        if (last - marks.last() < step / 2 + 1 && marks.size() > 1)
            marks.removeLast();
        marks << last;
    }
    foreach (int i, marks) {
        QVariantMap m = (i < n1 ? m_past[i] : m_future[i - n1]).toMap();
        bool isToday = i == n1;
        p.setPen(isToday ? INK : GRAY);
        QRectF r(X(i) - slot * step / 2, area.bottom() + px(6), slot * step, px(20));
        if (r.left() < area.left())
            r.moveLeft(area.left());
        if (r.right() > width())
            r.moveRight(width());
        p.drawText(r, Qt::AlignHCenter | Qt::AlignTop, m.value("label").toString());
        p.setPen(GRID);
        p.drawLine(QPointF(X(i), area.bottom()), QPointF(X(i), area.bottom() + px(4)));
    }
}
