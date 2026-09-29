#ifndef SALESCHART_H
#define SALESCHART_H

#include <QWidget>
#include <QVariant>

// 매출 누적 막대그래프 (카드 + 얼굴인식). 막대에 마우스를 올리면 금액이 표시된다.
// 외부 라이브러리 없이 QPainter로 그리므로 Qt4 / Qt5 모두 동작한다.
class SalesChart : public QWidget
{
    Q_OBJECT
public:
    explicit SalesChart(QWidget *parent = 0);

    // bars: 왼쪽부터 그릴 순서대로 [{label(x축), period(툴팁 제목), card, face}, ...]
    void setData(const QVariantList &bars, const QString &title);

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void leaveEvent(QEvent *event);

private:
    struct Bar
    {
        QString label;
        QString period;
        int card;
        int face;
    };

    QRectF plotRect() const;
    int barAt(const QPoint &pos) const;

    QList<Bar> m_bars;
    QString m_title;
    double m_max;    // y축 최댓값 (눈금 단위로 올림)
    double m_step;   // y축 눈금 간격
    int m_maxTotal;
    int m_hover;
};

#endif // SALESCHART_H
