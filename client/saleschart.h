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

    // rows: 서버 SALES 결과(최신순). 앞에서부터 maxBars개를 오래된 순으로 그린다.
    void setData(const QVariantList &rows, int maxBars, const QString &title);

protected:
    void paintEvent(QPaintEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void leaveEvent(QEvent *event);

private:
    struct Bar
    {
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
    int m_hover;
};

#endif // SALESCHART_H
