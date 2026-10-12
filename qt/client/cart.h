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
    int shelf;   // 진열대 번호 (0 = 지정 안 함)
};

namespace Ui {
class Cart;
}

// 회원 화면 오른쪽에 붙어 있는 장바구니 패널 (가로 화면)
class Cart : public QWidget
{
    Q_OBJECT
public:
    explicit Cart(QWidget *parent = 0);
    ~Cart();
    void setUser(const QVariantMap &user, QList<Item> *items);

signals:
    void paid();   // 결제가 끝나면 (회원 화면이 처음 화면으로 돌아감)

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
    QList<Item> *m_items;   // 회원 화면(Member)이 가진 장바구니 목록
    QString m_cardUid;   // 마지막으로 태그된 카드 UID
};

#endif
