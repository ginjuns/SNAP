#ifndef CARTDIALOG_H
#define CARTDIALOG_H

#include <QDialog>
#include <QList>
#include <QVariant>

class QLabel;
class QTableWidget;

struct CartItem
{
    int productId;
    QString name;
    int price;
    int qty;
};

// 장바구니 + 결제수단(카드 / 얼굴인식) 선택 창. 결제가 끝나면 accept().
class CartDialog : public QDialog
{
    Q_OBJECT
public:
    CartDialog(const QVariantMap &user, QList<CartItem> *cart, QWidget *parent = 0);

private slots:
    void removeSelected();
    void payByCard();
    void payByFace();

private:
    void refresh();
    int total() const;
    bool pay(int userId, const QString &method, QVariantMap *result);

    QVariantMap m_user;
    QList<CartItem> *m_cart;
    QTableWidget *m_table;
    QLabel *m_total;
};

#endif // CARTDIALOG_H
