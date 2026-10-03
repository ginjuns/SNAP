#ifndef UTIL_H
#define UTIL_H

#include <QtWidgets>

inline double &uiScale()
{
    static double s = 1.0;
    return s;
}

inline int px(int v)
{
    return qRound(v * uiScale());
}

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

inline void stretch(QTableWidget *t)
{
    t->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

#endif
