#ifndef ADMINWIDGET_H
#define ADMINWIDGET_H

#include <QWidget>
#include <QVariant>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QTableWidget;
class SalesChart;

// 관리자 화면: 상품 추가/삭제 / 날짜·월 선택 매출 확인
class AdminWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AdminWidget(QWidget *parent = 0);
    void start(const QVariantMap &user);

signals:
    void finished();   // 로그아웃 -> 초기 화면으로

private slots:
    void chooseImage();
    void addProduct();
    void deleteProduct();
    void loadProducts();
    void loadSales();
    void onUnitChanged();
    void prevPeriod();
    void nextPeriod();

private:
    void clearForm();
    bool isMonthly() const;

    QLabel *m_welcome;
    // 상품 관리
    QLineEdit *m_name;
    QLineEdit *m_price;
    QLabel *m_preview;
    QByteArray m_imageData;   // PNG
    QTableWidget *m_products;
    // 매출 확인
    QComboBox *m_unit;
    QDateEdit *m_date;
    QLabel *m_summary;
    SalesChart *m_chart;
    QTableWidget *m_details;   // 구매 상세 내역
    QTableWidget *m_byBuyer;   // 구매자·상품별 합계
};

#endif // ADMINWIDGET_H
