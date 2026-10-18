#ifndef CART_H
#define CART_H

#include <QWidget>
#include <QList>
#include <QVariant>

struct Item
{
    int id;
    QString name;
    int price;
    int qty;
    int stock;
    int shelf;
};

namespace Ui {
class Cart;
}

class Cart : public QWidget
{
    Q_OBJECT
public:
    explicit Cart(QWidget *parent = 0);
    ~Cart();
    void setUser(const QVariantMap &user, QList<Item> *items);

signals:
    void paid();

public slots:
    void refresh();

private slots:
    void clearAll();
    void change();
    void payCard();
    void payFace();
    void onCard(const QString &uid) { m_cardUid = uid; }

private:
    int total() const;
    bool checkSoldOut();
    bool pay(int userId, const QString &method, QVariantMap *out, const QString &cardUid = QString());

    Ui::Cart *ui;
    QVariantMap m_user;
    QList<Item> *m_items;
    QString m_cardUid;
};

#endif
