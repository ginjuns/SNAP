#ifndef CART_H
#define CART_H

#include <QDialog>
#include <QList>
#include <QVariant>

struct Item
{
    int id;
    QString name;
    int price;
    int qty;
    int stock;
    int shelf;   // 진열대 번호 (0 = 지정 안 함)
};

namespace Ui {
class Cart;
}

class Cart : public QDialog
{
    Q_OBJECT
public:
    Cart(const QVariantMap &user, QList<Item> *items, QWidget *parent = 0);
    ~Cart();

private slots:
    void clearAll();
    void change();
    void refresh();
    void payCard();
    void payFace();

private:
    int total() const;
    bool checkSoldOut();
    bool pay(int userId, const QString &method, QVariantMap *out);

    Ui::Cart *ui;
    QVariantMap m_user;
    QList<Item> *m_items;
};

#endif
