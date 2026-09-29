#ifndef MEMBERWIDGET_H
#define MEMBERWIDGET_H

#include <QWidget>
#include <QVariant>

#include "cartdialog.h"

class QLabel;
class QScrollArea;

// 일반회원 화면: 상품을 한 줄에 3개씩 (사진, 이름, 가격, 구매 버튼) 표시
class MemberWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MemberWidget(QWidget *parent = 0);
    void start(const QVariantMap &user);

signals:
    void finished();   // 로그아웃 또는 결제 완료 -> 초기 화면으로

private slots:
    void onBuy();

private:
    void loadProducts();

    QLabel *m_welcome;
    QScrollArea *m_scroll;
    QVariantMap m_user;
    QVariantList m_products;
    QList<CartItem> m_cart;
};

#endif // MEMBERWIDGET_H
