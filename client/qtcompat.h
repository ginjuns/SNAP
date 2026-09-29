#ifndef QTCOMPAT_H
#define QTCOMPAT_H

// Qt4 / Qt5 양쪽에서 빌드되도록 차이를 모아둔 헤더
#include <QtGlobal>

#if QT_VERSION >= 0x050000
#include <QtWidgets>   // Qt5부터 위젯 클래스가 QtWidgets 모듈로 분리됨
#else
#include <QtGui>
#endif

// 테이블 열을 창 너비에 맞게 균등하게 늘린다.
inline void stretchColumns(QHeaderView *header)
{
#if QT_VERSION >= 0x050000
    header->setSectionResizeMode(QHeaderView::Stretch);
#else
    header->setResizeMode(QHeaderView::Stretch);
#endif
}

#endif // QTCOMPAT_H
