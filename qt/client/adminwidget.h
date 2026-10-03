#ifndef ADMINWIDGET_H
#define ADMINWIDGET_H

#include <QWidget>
#include <QVariant>

namespace Ui {
class AdminWidget;
}

// 관리자 화면: 상품 추가/삭제 / 날짜·월 선택 매출 확인
class AdminWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AdminWidget(QWidget *parent = 0);
    ~AdminWidget();
    void start(const QVariantMap &user);

signals:
    void finished();   // 로그아웃 -> 초기 화면으로

protected:
    bool eventFilter(QObject *watched, QEvent *event);   // 사진 영역 터치 -> chooseImage()

private slots:
    void chooseImage();
    void addProduct();
    void updateProduct();
    void deleteProduct();
    void onProductSelected();
    void clearForm();
    void loadProducts();
    void loadSales();
    void onUnitChanged();
    void prevPeriod();
    void nextPeriod();
    void goToday();

private:
    int selectedRow() const;
    bool readForm(QString *name, int *price, int *stock);
    bool isMonthly() const;

    Ui::AdminWidget *ui;
    // 상품 관리
    QByteArray m_imageData;   // PNG
    bool m_imageChanged;      // 수정 시 사진을 새로 골랐는지
    QVariantList m_productList;   // 표(productTable)의 행 순서와 같음
};

#endif // ADMINWIDGET_H
