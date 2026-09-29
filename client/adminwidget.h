#ifndef ADMINWIDGET_H
#define ADMINWIDGET_H

#include <QWidget>
#include <QVariant>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class SalesChart;

// 관리자 화면: 상품 추가 / 일별·월별 매출 확인
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

private:
    void clearForm();

    QLabel *m_welcome;
    // 상품 관리
    QLineEdit *m_name;
    QLineEdit *m_price;
    QLabel *m_preview;
    QByteArray m_imageData;   // PNG
    QTableWidget *m_products;
    // 매출 확인
    QComboBox *m_unit;
    SalesChart *m_chart;
    QTableWidget *m_sales;
    QLabel *m_salesTotal;
};

#endif // ADMINWIDGET_H
