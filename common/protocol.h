#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QByteArray>
#include <QDataStream>
#include <QVariant>
#include <QtEndian>

// 키오스크 <-> DB 서버 통신 포트
const quint16 KIOSK_PORT = 9000;

// 메시지 형식: [4바이트 길이(big-endian)] + [QDataStream으로 직렬화한 QVariantList]
//   요청: [명령, 인자1, 인자2, ...]
//   응답: [성공여부(bool), 결과 데이터 또는 오류 메시지]
//
// 명령 목록
//   LOGIN       (userId)                                   -> {id, name, role, balance}
//   PRODUCTS    ()                                         -> [{id, name, price, image}, ...]
//   ADD_PRODUCT (name, price, imagePngBytes)               -> 없음
//   DELETE_PRODUCT (productId)                             -> 없음
//   PAY         (userId, "card"|"face", [{productId, qty}]) -> {total, balance}
//   SALES       ("day", "yyyy-MM-dd") | ("month", "yyyy-MM")
//               -> {buckets: [{bucket(시 또는 일), card, face}],
//                   details: [{soldAt, buyer, product, qty, amount, method}]}

inline QByteArray packMessage(const QVariantList &msg)
{
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_6);
    out << msg;

    uchar header[4];
    qToBigEndian<quint32>(payload.size(), header);
    return QByteArray(reinterpret_cast<const char *>(header), 4) + payload;
}

// buffer에 완성된 메시지가 있으면 꺼내서 msg에 담고 true를 반환한다.
inline bool unpackMessage(QByteArray &buffer, QVariantList *msg)
{
    if (buffer.size() < 4)
        return false;
    const quint32 size = qFromBigEndian<quint32>(reinterpret_cast<const uchar *>(buffer.constData()));
    if (quint32(buffer.size()) < 4 + size)
        return false;

    QDataStream in(buffer.mid(4, size));
    in.setVersion(QDataStream::Qt_4_6);
    in >> *msg;
    buffer.remove(0, 4 + size);
    return true;
}

#endif // PROTOCOL_H
