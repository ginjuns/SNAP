#include "saleschart.h"
#include "serverclient.h"

#include "qtcompat.h"

#include <cmath>

namespace {
const QColor kSurface("#fcfcfb");
const QColor kInk("#0b0b0b");
const QColor kInkSecondary("#52514e");
const QColor kMuted("#898781");
const QColor kGrid("#e1e0d9");
const QColor kBaseline("#c3c2b7");
const QColor kCard("#2a78d6");   // 계열 1: 카드
const QColor kFace("#eb6834");   // 계열 2: 얼굴인식

const double kGap = 2;           // 누적 막대 사이 간격(px)
const double kRadius = 4;        // 막대 윗부분 둥글기(px)

// 윗모서리만 둥근 사각형
QPainterPath topRounded(const QRectF &r)
{
    const double rad = qMin(kRadius, qMin(r.width() / 2, r.height()));
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

QFont pixelFont(const QFont &base, int px, bool bold = false)
{
    QFont f(base);
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}
} // namespace

SalesChart::SalesChart(QWidget *parent)
    : QWidget(parent), m_max(0), m_step(0), m_maxTotal(0), m_hover(-1)
{
    setMouseTracking(true);
    setMinimumHeight(300);
}

void SalesChart::setData(const QVariantList &bars, const QString &title)
{
    m_title = title;
    m_bars.clear();
    foreach (const QVariant &v, bars) {
        const QVariantMap r = v.toMap();
        Bar bar = { r.value("label").toString(), r.value("period").toString(),
                    r.value("card").toInt(), r.value("face").toInt() };
        m_bars << bar;
    }

    // 최댓값을 1·2·5 단위의 보기 좋은 눈금으로 맞춘다 (눈금 약 4칸)
    m_maxTotal = 0;
    foreach (const Bar &b, m_bars)
        m_maxTotal = qMax(m_maxTotal, b.card + b.face);
    const double raw = qMax(m_maxTotal, 1000) / 4.0;
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    const double n = raw / magnitude;
    m_step = (n <= 1 ? 1 : n <= 2 ? 2 : n <= 5 ? 5 : 10) * magnitude;
    m_max = std::ceil(qMax(m_maxTotal, 1) / m_step) * m_step;

    m_hover = -1;
    update();
}

QRectF SalesChart::plotRect() const
{
    return QRectF(80, 64, width() - 80 - 24, height() - 64 - 40);
}

int SalesChart::barAt(const QPoint &pos) const
{
    const QRectF plot = plotRect();
    if (m_bars.isEmpty() || !plot.adjusted(0, -20, 0, 30).contains(pos))
        return -1;
    const int i = int((pos.x() - plot.left()) / (plot.width() / m_bars.size()));
    return (i >= 0 && i < m_bars.size()) ? i : -1;
}

void SalesChart::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), kSurface);

    // 제목
    p.setPen(kInk);
    p.setFont(pixelFont(font(), 17, true));
    p.drawText(QRectF(16, 10, width() - 32, 26), Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // 범례 (계열이 2개이므로 항상 표시)
    p.setFont(pixelFont(font(), 13));
    const QString labels[2] = { "카드", "얼굴인식" };
    const QColor colors[2] = { kCard, kFace };
    double x = 16;
    for (int i = 0; i < 2; ++i) {
        p.setPen(Qt::NoPen);
        p.setBrush(colors[i]);
        p.drawRoundedRect(QRectF(x, 42, 12, 12), 2, 2);
        p.setPen(kInkSecondary);
        const double w = p.fontMetrics().width(labels[i]);
        p.drawText(QRectF(x + 18, 38, w + 4, 20), Qt::AlignLeft | Qt::AlignVCenter, labels[i]);
        x += 18 + w + 20;
    }

    const QRectF plot = plotRect();
    if (m_maxTotal == 0) {
        p.setPen(kMuted);
        p.drawText(plot, Qt::AlignCenter, "선택한 기간에 매출이 없습니다.");
        return;
    }

    // y축 눈금과 격자선
    p.setFont(pixelFont(font(), 12));
    for (double v = 0; v <= m_max + 0.5; v += m_step) {
        const double y = plot.bottom() - v / m_max * plot.height();
        p.setPen(QPen(v == 0 ? kBaseline : kGrid, 1));
        p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        p.setPen(kMuted);
        p.drawText(QRectF(0, y - 10, plot.left() - 10, 20), Qt::AlignRight | Qt::AlignVCenter,
                   QString("%L1").arg(qint64(v)));
    }

    // 막대
    const int n = m_bars.size();
    const double slot = plot.width() / n;
    const double barWidth = qMin(48.0, slot * 0.6);
    const int labelEvery = qMax(1, int(std::ceil(n * 64.0 / plot.width())));   // x축 라벨 겹침 방지

    int peak = 0;
    for (int i = 1; i < n; ++i)
        if (m_bars[i].card + m_bars[i].face > m_bars[peak].card + m_bars[peak].face)
            peak = i;

    for (int i = 0; i < n; ++i) {
        const Bar &b = m_bars[i];
        const double cx = plot.left() + slot * (i + 0.5);
        const double left = cx - barWidth / 2;

        if (i == m_hover) {   // 마우스가 올라간 기간 강조
            QColor hl(kGrid);
            hl.setAlpha(110);
            p.fillRect(QRectF(plot.left() + slot * i, plot.top(), slot, plot.height()), hl);
        }

        const double hCard = b.card / m_max * plot.height();
        const double hFace = b.face / m_max * plot.height();
        const bool both = b.card > 0 && b.face > 0;
        p.setPen(Qt::NoPen);
        if (b.card > 0) {
            const QRectF r(left, plot.bottom() - hCard, barWidth, hCard);
            p.setBrush(kCard);
            if (b.face > 0)
                p.drawRect(r);
            else
                p.drawPath(topRounded(r));
        }
        if (b.face > 0) {
            const double bottom = plot.bottom() - hCard - (both ? kGap : 0);
            p.setBrush(kFace);
            p.drawPath(topRounded(QRectF(left, bottom - hFace, barWidth, hFace)));
        }

        // 최고 매출 기간에만 합계 표시 (나머지는 마우스 오버/표에서 확인)
        if (i == peak) {
            p.setPen(kInkSecondary);
            p.setFont(pixelFont(font(), 12, true));
            const double top = plot.bottom() - hCard - hFace - (both ? kGap : 0);
            p.drawText(QRectF(cx - 60, top - 20, 120, 18), Qt::AlignCenter, won(b.card + b.face));
        }

        // x축 라벨 (촘촘하면 일부만 표시)
        if (i % labelEvery == 0) {
            p.setPen(kMuted);
            p.setFont(pixelFont(font(), 12));
            p.drawText(QRectF(cx - 40, plot.bottom() + 6, 80, 20), Qt::AlignHCenter | Qt::AlignTop, b.label);
        }
    }
}

void SalesChart::mouseMoveEvent(QMouseEvent *event)
{
    int i = barAt(event->pos());
    if (i >= 0 && m_bars[i].card + m_bars[i].face == 0)   // 매출 없는 칸은 툴팁 생략
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
    QToolTip::showText(event->globalPos(),
                       QString("<b>%1</b><br>카드: %2<br>얼굴인식: %3<br><b>합계: %4</b>")
                           .arg(b.period).arg(won(b.card)).arg(won(b.face)).arg(won(b.card + b.face)),
                       this);
}

void SalesChart::leaveEvent(QEvent *)
{
    m_hover = -1;
    QToolTip::hideText();
    update();
}
