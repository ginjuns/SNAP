#ifndef CARTDIALOG_H
#define CARTDIALOG_H

#include <QDialog>
#include <QList>
#include <QVariant>

namespace Ui {
class CartDialog;
}

struct CartItem
{
    int productId;
    QString name;
    int price;
    int qty;
    int stock;   // 담을 수 있는 최대 수량 (최종 확인은 결제 시 서버에서)
};

// 장바구니 + 결제수단(카드 / 얼굴인식) 선택 창. 결제가 끝나면 accept().
class CartDialog : public QDialog
{
    Q_OBJECT
public:
    CartDialog(const QVariantMap &user, QList<CartItem> *cart, QWidget *parent = 0);
    ~CartDialog();

private slots:
    void clearCart();
    void changeQty();   // 행의 [-] / [+] 버튼
    void refresh();
    void payByCard();
    void payByFace();

private:
    int total() const;
    bool pay(int userId, const QString &method, QVariantMap *result);

    Ui::CartDialog *ui;
    QVariantMap m_user;
    QList<CartItem> *m_cart;
};

#endif // CARTDIALOG_H
