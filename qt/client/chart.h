#ifndef CHART_H
#define CHART_H

#include <QWidget>
#include <QVariant>

class Chart : public QWidget
{
    Q_OBJECT
public:
    explicit Chart(QWidget *parent = 0);

    void setData(const QVariantList &bars, const QString &title);

protected:
    void paintEvent(QPaintEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void leaveEvent(QEvent *e);

private:
    struct Bar
    {
        QString label;
        QString period;
        int card;
        int face;
    };

    QRectF area() const;
    int barAt(const QPoint &pos) const;
    void showTip(QMouseEvent *e);

    QList<Bar> m_bars;
    QString m_title;
    double m_max;
    double m_step;
    int m_top;
    int m_hover;
};

#endif
