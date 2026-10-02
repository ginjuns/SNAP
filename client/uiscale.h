#ifndef UISCALE_H
#define UISCALE_H

#include <QRegularExpression>
#include <QString>

// 화면 배율. 모든 크기는 세로 720x1280 화면 기준으로 적고, 실제 모니터에 맞춰 곱해서 쓴다.
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

#endif // UISCALE_H
