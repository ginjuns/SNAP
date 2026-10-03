#include "chart.h"
#include "net.h"
#include "util.h"

#include <cmath>

namespace {

const QColor BG("#fcfcfb");
const QColor INK("#0b0b0b");
const QColor INK2("#52514e");
const QColor GRAY("#898781");
const QColor GRID("#e1e0d9");
const QColor LINE("#c3c2b7");
const QColor CARD("#2a78d6");
const QColor FACE("#eb6834");

const double GAP = 2;
const double RADIUS = 4;

QPainterPath roundTop(const QRectF &r)
{
    double rad = qMin(RADIUS, qMin(r.width() / 2, r.height()));
    QPainterPath path;
    path.moveTo(r.bottomLeft());
    path.lineTo(r.left(), r.top() + rad);
    path.quadTo(r.topLeft(), QPointF(r.left() + rad, r.top()));
    path.lineTo(r.right() - rad, r.top());
    path.quadTo(r.topRight(), QPointF(r.right(), r.top() + rad));
    path.lineTo(r.bottomRight());
    path.closeSubpath();
    return path;
}

QFont pxFont(const QFont &base, int size, bool bold = false)
{
    QFont f(base);
    f.setPixelSize(px(size));
    f.setBold(bold);
    return f;
}

}

Chart::Chart(QWidget *parent)
    : QWidget(parent), m_max(0), m_step(0), m_top(0), m_hover(-1)
{
}

void Chart::setData(const QVariantList &bars, const QString &title)
{
    m_title = title;
    m_bars.clear();
    foreach (const QVariant &v, bars) {
        QVariantMap m = v.toMap();
        Bar b = { m.value("label").toString(), m.value("period").toString(),
                  m.value("card").toInt(), m.value("face").toInt() };
        m_bars << b;
    }

    m_top = 0;
    foreach (const Bar &b, m_bars)
        m_top = qMax(m_top, b.card + b.face);
    double raw = qMax(m_top, 1000) / 4.0;
    double mag = std::pow(10.0, std::floor(std::log10(raw)));
    double n = raw / mag;
    m_step = (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * mag;
    m_max = std::ceil(qMax(m_top, 1) / m_step) * m_step;

    m_hover = -1;
    update();
}

QRectF Chart::area() const
{
    return QRectF(80, 64, width() - 80 - 24, height() - 64 - 40);
}

int Chart::barAt(const QPoint &pos) const
{
    QRectF a = area();
    if (m_bars.isEmpty() || !a.adjusted(0, -20, 0, 30).contains(pos))
        return -1;
    int i = int((pos.x() - a.left()) / (a.width() / m_bars.size()));
    return (i >= 0 && i < m_bars.size()) ? i : -1;
}

void Chart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), BG);

    p.setPen(INK);
    p.setFont(pxFont(font(), 17, true));
    p.drawText(QRectF(16, 10, width() - 32, 26), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    p.setFont(pxFont(font(), 13));
    const QString names[2] = { "카드", "얼굴인식" };
    const QColor colors[2] = { CARD, FACE };
    double x = 16;
    for (int i = 0; i < 2; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(colors[i]);
        p.drawRoundedRect(QRectF(x, 42, 12, 12), 2, 2);
        p.setPen(INK2);
        double w = p.fontMetrics().width(names[i]);
        p.drawText(QRectF(x + 18, 38, w + 4, 20), Qt::AlignLeft | Qt::AlignVCenter, names[i]);
        x += 18 + w + 20;
    }

    QRectF a = area();
    if (m_top == 0) {
        p.setPen(GRAY);
        p.drawText(a, Qt::AlignCenter, "선택한 기간에 매출이 없습니다.");
        return;
    }

    p.setFont(pxFont(font(), 12));
    for (double v = 0; v <= m_max + 0.5; v += m_step) {
        double y = a.bottom() - v / m_max * a.height();
        p.setPen(QPen(v == 0 ? LINE : GRID, 1));
        p.drawLine(QPointF(a.left(), y), QPointF(a.right(), y));
        p.setPen(GRAY);
        p.drawText(QRectF(0, y - 10, a.left() - 10, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%L1").arg(qint64(v)));
    }

    int n = m_bars.size();
    double slot = a.width() / n;
    double bw = qMin(48.0, slot * 0.6);
    int every = qMax(1, int(std::ceil(n * 64.0 / a.width())));

    int peak = 0;
    for (int i = 1; i < n; ++i)
        if (m_bars[i].card + m_bars[i].face > m_bars[peak].card + m_bars[peak].face)
            peak = i;

    for (int i = 0; i < n; ++i) {
        const Bar &b = m_bars[i];
        double cx = a.left() + slot * (i + 0.5);
        double left = cx - bw / 2;

        if (i == m_hover) {
            QColor hl(GRID);
            hl.setAlpha(110);
            p.fillRect(QRectF(a.left() + slot * i, a.top(), slot, a.height()), hl);
        }

        double h1 = b.card / m_max * a.height();
        double h2 = b.face / m_max * a.height();
        bool both = b.card > 0 && b.face > 0;
        p.setPen(Qt::NoPen);
        if (b.card > 0) {
            QRectF r(left, a.bottom() - h1, bw, h1);
            p.setBrush(CARD);
            if (b.face > 0)
                p.drawRect(r);
            else
                p.drawPath(roundTop(r));
        }
        if (b.face > 0) {
            double bottom = a.bottom() - h1 - (both ? GAP : 0);
            p.setBrush(FACE);
            p.drawPath(roundTop(QRectF(left, bottom - h2, bw, h2)));
        }

        if (i == peak) {
            p.setPen(INK2);
            p.setFont(pxFont(font(), 12, true));
            double top = a.bottom() - h1 - h2 - (both ? GAP : 0);
            p.drawText(QRectF(cx - 60, top - 20, 120, 18), Qt::AlignCenter, won(b.card + b.face));
        }

        if (i % every == 0) {
            p.setPen(GRAY);
            p.setFont(pxFont(font(), 12));
            p.drawText(QRectF(cx - 40, a.bottom() + 6, 80, 20), Qt::AlignHCenter | Qt::AlignTop, b.label);
        }
    }
}

void Chart::mousePressEvent(QMouseEvent *e)
{
    showTip(e);
}

void Chart::mouseMoveEvent(QMouseEvent *e)
{
    showTip(e);
}

void Chart::showTip(QMouseEvent *e)
{
    int i = barAt(e->pos());
    if (i >= 0 && m_bars[i].card + m_bars[i].face == 0)
        i = -1;
    if (i != m_hover) {
        m_hover = i;
        update();
    }
    if (i < 0) {
        QToolTip::hideText();
        return;
    }
    const Bar &b = m_bars[i];
    QToolTip::showText(e->globalPos(),
                       QString("<b>%1</b><br>카드: %2<br>얼굴인식: %3<br><b>합계: %4</b>")
                           .arg(b.period).arg(won(b.card)).arg(won(b.face)).arg(won(b.card + b.face)),
                       this);
}

void Chart::leaveEvent(QEvent *)
{
    m_hover = -1;
    QToolTip::hideText();
    update();
}
