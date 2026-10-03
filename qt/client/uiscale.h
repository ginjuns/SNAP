#ifndef UISCALE_H
#define UISCALE_H

#include <QLayout>
#include <QRegularExpression>
#include <QString>
#include <QWidget>

// 화면 배율. 모든 크기(.ui 포함)는 세로 720x1280 화면 기준으로 적고, 실제 모니터에 맞춰 곱해서 쓴다.
//   15.6인치 세로(1080x1920) -> 1.5배. 값은 main()에서 모니터 크기로 정한다.
// (Qt의 QT_SCALE_FACTOR는 일부 환경에서 화면이 하얗게만 나와서 쓰지 않는다)
inline double &uiScale()
{
    static double scale = 1.0;
    return scale;
}

// 기준 크기(px) -> 실제 크기(px)
inline int px(int value)
{
    return qRound(value * uiScale());
}

// 스타일시트 안의 "16px" 같은 값을 배율에 맞게 바꾼다.
inline QString css(const QString &style)
{
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = QRegularExpression("(\\d+)px").globalMatch(style);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += style.mid(last, m.capturedStart() - last) + QString::number(px(m.captured(1).toInt())) + "px";
        last = m.capturedEnd();
    }
    return out + style.mid(last);
}

// Qt Designer(.ui)로 만든 화면을 배율에 맞춘다. setupUi() 바로 뒤에 한 번 부른다.
// .ui 에는 기준 크기로 적어 둔다: 최소/최대 크기, 스타일시트의 px, 고정 간격(spacer)
inline void scaleUi(QWidget *root)
{
    QList<QWidget *> widgets = root->findChildren<QWidget *>();
    widgets.prepend(root);
    foreach (QWidget *w, widgets) {
        if (w->objectName().isEmpty() || w->objectName().startsWith("qt_"))
            continue;   // Qt 내부 위젯(스크롤바, 탭바 등)은 그대로 둔다
        if (!w->styleSheet().isEmpty())
            w->setStyleSheet(css(w->styleSheet()));
        const QSize min = w->minimumSize();
        const QSize max = w->maximumSize();
        w->setMinimumSize(px(min.width()), px(min.height()));
        w->setMaximumSize(max.width() < QWIDGETSIZE_MAX ? px(max.width()) : QWIDGETSIZE_MAX,
                          max.height() < QWIDGETSIZE_MAX ? px(max.height()) : QWIDGETSIZE_MAX);
    }
    foreach (QLayout *layout, root->findChildren<QLayout *>()) {
        for (int i = 0; i < layout->count(); ++i) {
            QSpacerItem *spacer = layout->itemAt(i)->spacerItem();
            if (!spacer)
                continue;
            const QSize hint = spacer->sizeHint();
            const QSizePolicy policy = spacer->sizePolicy();
            spacer->changeSize(px(hint.width()), px(hint.height()), policy.horizontalPolicy(), policy.verticalPolicy());
        }
        layout->invalidate();
    }
}

#endif // UISCALE_H
