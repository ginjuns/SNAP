/*
 * 화면 공통 도우미
 *
 * - uiScale/px/css/scaleUi: 모니터 크기에 맞춰 위젯, 글자, 스타일시트 크기 조정
 * - touchScroll: 손가락으로 끌어서 스크롤
 * - stretch: 표의 열을 가로로 꽉 채움
 */

#ifndef UTIL_H
#define UTIL_H

#include <QtWidgets>

// 화면 배율 (main 에서 모니터 크기로 정함)
inline double &uiScale()
{
    static double s = 1.0;
    return s;
}

// 디자인 기준 크기(px)를 화면 배율에 맞게 변환
inline int px(int v)
{
    return qRound(v * uiScale());
}

// 스타일시트 안의 'Npx' 값을 모두 화면 배율에 맞게 바꿈
inline QString css(const QString &s)
{
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = QRegularExpression("(\\d+)px").globalMatch(s);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        out += s.mid(last, m.capturedStart() - last) + QString::number(px(m.captured(1).toInt())) + "px";
        last = m.capturedEnd();
    }
    return out + s.mid(last);
}

// .ui 로 만든 화면의 위젯 크기, 여백, 스타일시트를 화면 배율에 맞게 키움
inline void scaleUi(QWidget *root)
{
    QList<QWidget *> list = root->findChildren<QWidget *>();
    list.prepend(root);
    foreach (QWidget *w, list) {
        if (w->objectName().isEmpty() || w->objectName().startsWith("qt_"))
            continue;
        if (!w->styleSheet().isEmpty())
            w->setStyleSheet(css(w->styleSheet()));
        QSize min = w->minimumSize();
        QSize max = w->maximumSize();
        w->setMinimumSize(px(min.width()), px(min.height()));
        w->setMaximumSize(max.width() < QWIDGETSIZE_MAX ? px(max.width()) : QWIDGETSIZE_MAX,
                          max.height() < QWIDGETSIZE_MAX ? px(max.height()) : QWIDGETSIZE_MAX);
    }
    foreach (QLayout *l, root->findChildren<QLayout *>()) {
        for (int i = 0; i < l->count(); ++i) {
            QSpacerItem *sp = l->itemAt(i)->spacerItem();
            if (!sp)
                continue;
            QSize h = sp->sizeHint();
            QSizePolicy p = sp->sizePolicy();
            sp->changeSize(px(h.width()), px(h.height()), p.horizontalPolicy(), p.verticalPolicy());
        }
        l->invalidate();
    }
}

// 손가락으로 끌어서 스크롤되게 함 (끝에서 튕기는 효과 없음)
inline void touchScroll(QAbstractScrollArea *area)
{
    if (QAbstractItemView *v = qobject_cast<QAbstractItemView *>(area)) {
        v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    QScroller::grabGesture(area->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller *s = QScroller::scroller(area->viewport());
    QScrollerProperties sp = s->scrollerProperties();
    sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                       QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff));
    sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                       QVariant::fromValue(QScrollerProperties::OvershootAlwaysOff));
    s->setScrollerProperties(sp);
}

// 표의 열을 가로로 꽉 채움
inline void stretch(QTableWidget *t)
{
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

#endif
