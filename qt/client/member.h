#ifndef MEMBER_H
#define MEMBER_H

#include <QWidget>
#include <QVariant>

#include "cart.h"

namespace Ui {
class Member;
}

class Member : public QWidget
{
    Q_OBJECT
public:
    explicit Member(QWidget *parent = 0);
    ~Member();
    void start(const QVariantMap &user);

signals:
    void done();

protected:
    bool eventFilter(QObject *obj, QEvent *e);

private slots:
    void openCart();

private:
    void load();
    void add(int i);
    void updateBar();

    Ui::Member *ui;
    QVariantMap m_user;
    QVariantList m_list;
    QList<Item> m_cart;
};

#endif
