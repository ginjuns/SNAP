#ifndef ADMIN_H
#define ADMIN_H

#include <QWidget>
#include <QVariant>

namespace Ui {
class Admin;
}
class QLabel;
class QDateEdit;

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
    void onReportUnit();

private:
    void pickDate(QDateEdit *edit);
    int row() const;
    bool readForm(QString *name, int *price, int *stock);
    bool monthly() const;

    void loadTargets();
    QLabel *addBubble(const QString &text, bool mine);
    void setFollowUps(const QStringList &list);
    void flushChat();

    Ui::Admin *ui;
    QByteArray m_img;
    bool m_imgChanged;
    QVariantList m_list;
    QVariantList m_members;
    bool m_asking = false;   // AI 상담 답을 기다리는 중 (그동안 다른 질문은 무시)
    int m_chat = 0;          // 대화를 지울 때마다 증가 (지운 뒤 도착한 답은 버림)
};

#endif
