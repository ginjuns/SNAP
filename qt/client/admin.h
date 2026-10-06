#ifndef ADMIN_H
#define ADMIN_H

#include <QWidget>
#include <QVariant>

namespace Ui {
class Admin;
}

class Admin : public QWidget
{
    Q_OBJECT
public:
    explicit Admin(QWidget *parent = 0);
    ~Admin();
    void start(const QVariantMap &user);

signals:
    void done();

protected:
    bool eventFilter(QObject *obj, QEvent *e);

private slots:
    void pickImage();
    void add();
    void edit();
    void del();
    void onSelect();
    void reset();
    void loadProducts();
    void loadSales();
    void onUnit();
    void prev();
    void next();
    void today();
    void makeReport();
    void onCategory();
    void ask();
    void forecast();

private:
    void pickDate();
    int row() const;
    bool readForm(QString *name, int *price, int *stock);
    bool monthly() const;

    void loadTargets();
    void addBubble(const QString &text, bool mine);
    void setFollowUps(const QStringList &list);

    Ui::Admin *ui;
    QByteArray m_img;
    bool m_imgChanged;
    QVariantList m_list;
    QVariantList m_members;
};

#endif
