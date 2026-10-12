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

// 손가락으로 끌어서 스크롤되게 함
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

// SNAP 색상 (메인 색 #cdffcd 를 기준으로 한 민트/초록 계열)
namespace Theme {
const char *const MINT  = "#cdffcd";   // 메인 색: 첫 화면 배경, 표 머리, 선택/눌림 표시
const char *const LIGHT = "#f2fff2";   // 화면 배경
const char *const LINE  = "#a8dca8";   // 테두리
const char *const GREEN = "#1f7a43";   // 강조 버튼, 카드 매출 (흰 글자와 대비 확보)
const char *const DEEP  = "#14532d";   // 로고, 강조 글자
const char *const INK   = "#17301f";   // 기본 글자
const char *const RED   = "#d32f2f";   // 품절, 경고
}

// 프로그램 전체 공통 스타일시트 (SNAP 테마)
//  - 강조 버튼: 로그인 시작, 장바구니, 결제, 확인, AI 생성 버튼 (objectName 으로 지정)
inline QString appStyle()
{
    QString s =
        "QWidget { font-size: 16px; color: INK; }"
        "QMainWindow, QDialog, Member, Admin { background: LIGHT; }"
        "QLabel { background: transparent; }"

        "QPushButton { min-height: 40px; padding: 4px 16px; background: white; color: INK;"
        "  border: 2px solid LINE; border-radius: 10px; }"
        "QPushButton:pressed, QPushButton:checked { background: MINT; border-color: GREEN; }"
        "QPushButton:disabled { color: #9bb39f; border-color: #d6ecd6; }"
        "QPushButton:focus { outline: none; }"
        "QPushButton#btnLogin, QPushButton#btnCart, QPushButton#btnOk, QPushButton#btnCard,"
        "QPushButton#btnFace, QPushButton#btnReport, QPushButton#btnForecast {"
        "  background: GREEN; color: white; font-weight: bold; border: none; }"
        "QPushButton#btnLogin:pressed, QPushButton#btnCart:pressed, QPushButton#btnOk:pressed,"
        "QPushButton#btnCard:pressed, QPushButton#btnFace:pressed, QPushButton#btnReport:pressed,"
        "QPushButton#btnForecast:pressed { background: DEEP; }"

        "QLineEdit, QComboBox, QDateEdit { background: white; border: 2px solid LINE; border-radius: 8px;"
        "  padding: 4px 8px; min-height: 32px; }"
        "QLineEdit:focus { border-color: GREEN; }"
        "QComboBox::drop-down { border: none; width: 28px; }"
        "QComboBox QAbstractItemView { background: white; selection-background-color: MINT; selection-color: INK; }"

        "QTableWidget, QTextBrowser { background: white; border: 1px solid LINE; border-radius: 6px;"
        "  gridline-color: #e2f4e2; selection-background-color: MINT; selection-color: INK; }"
        "QHeaderView::section { background: MINT; color: DEEP; font-weight: bold; padding: 6px;"
        "  border: none; border-right: 1px solid LINE; border-bottom: 1px solid LINE; }"
        "QTableCornerButton::section { background: MINT; border: none; }"

        "QTabWidget::pane { border: 1px solid LINE; border-radius: 6px; background: white; top: -1px; }"
        "QTabBar::tab { background: #e4fbe4; color: INK; padding: 10px 18px; margin-right: 4px;"
        "  border: 1px solid LINE; border-bottom: none; border-top-left-radius: 8px; border-top-right-radius: 8px; }"
        "QTabBar::tab:selected { background: MINT; color: DEEP; font-weight: bold; }"

        "QLabel#lblImage { background: white; border: 2px dashed LINE; border-radius: 10px; color: #5b7d65; }"
        "QScrollArea { border: none; background: transparent; }"
        "QScrollArea > QWidget > QWidget { background: transparent; }"
        "QScrollBar:vertical { width: 32px; background: #e4fbe4; margin: 0; }"
        "QScrollBar:horizontal { height: 32px; background: #e4fbe4; margin: 0; }"
        "QScrollBar::handle:vertical { background: LINE; min-height: 60px; border-radius: 8px; margin: 4px; }"
        "QScrollBar::handle:horizontal { background: LINE; min-width: 60px; border-radius: 8px; margin: 4px; }"
        "QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: none; }"
        "QToolTip { background: white; color: INK; border: 1px solid LINE; }";
    s.replace("MINT", Theme::MINT).replace("LIGHT", Theme::LIGHT).replace("LINE", Theme::LINE)
        .replace("GREEN", Theme::GREEN).replace("DEEP", Theme::DEEP).replace("INK", Theme::INK);
    return css(s);
}

#endif
