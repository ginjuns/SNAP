#ifndef MEMBERWIDGET_H
#define MEMBERWIDGET_H

#include <QWidget>
#include <QVariant>

#include "cartdialog.h"

namespace Ui {
class MemberWidget;
}

// 일반회원 화면
//   상품을 한 줄에 3개씩 (사진, 이름, 가격) 표시. 상품 카드를 누르면 장바구니에 담긴다.
//   하단 바: 총 수량 / 총 금액 / [장바구니]
class MemberWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MemberWidget(QWidget *parent = 0);
    ~MemberWidget();
    void start(const QVariantMap &user);

signals:
    void finished();   // 처음으로 또는 결제 완료 -> 초기 화면으로

protected:
    bool eventFilter(QObject *watched, QEvent *event);   // 상품 카드 터치

private slots:
    void openCart();

private:
    void loadProducts();
    void addToCart(int index);
    void updateCartBar();

    Ui::MemberWidget *ui;   // noticeLabel: "'아메리카노'를 담았습니다"
    QVariantMap m_user;
    QVariantList m_products;
    QList<CartItem> m_cart;
};

#endif // MEMBERWIDGET_H
