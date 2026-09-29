#include "adminwidget.h"
#include "saleschart.h"
#include "serverclient.h"

#include "qtcompat.h"

static QTableWidget *makeTable(const QStringList &headers)
{
    QTableWidget *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    stretchColumns(table->horizontalHeader());
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    return table;
}

AdminWidget::AdminWidget(QWidget *parent)
    : QWidget(parent)
{
    m_welcome = new QLabel;
    m_welcome->setStyleSheet("font-size: 22px; font-weight: bold;");
    QPushButton *logout = new QPushButton("로그아웃");
    connect(logout, SIGNAL(clicked()), SIGNAL(finished()));

    QHBoxLayout *header = new QHBoxLayout;
    header->addWidget(m_welcome);
    header->addStretch();
    header->addWidget(logout);

    // ---- 상품 관리 탭 ----
    m_name = new QLineEdit;
    m_price = new QLineEdit;
    m_price->setValidator(new QIntValidator(1, 100000000, m_price));   // 숫자만 입력
    m_price->setPlaceholderText("숫자만 입력 (예: 3000)");
    m_preview = new QLabel("이미지 없음");
    m_preview->setFixedSize(150, 150);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setFrameShape(QFrame::StyledPanel);
    QPushButton *imageButton = new QPushButton("이미지 선택...");
    QPushButton *addButton = new QPushButton("상품 추가");
    connect(imageButton, SIGNAL(clicked()), SLOT(chooseImage()));
    connect(addButton, SIGNAL(clicked()), SLOT(addProduct()));

    QFormLayout *form = new QFormLayout;
    form->addRow("상품명", m_name);
    form->addRow("가격(원)", m_price);
    form->addRow("사진", m_preview);
    form->addRow("", imageButton);
    form->addRow("", addButton);

    m_products = makeTable(QStringList() << "ID" << "상품명" << "가격");
    m_products->setSelectionMode(QAbstractItemView::SingleSelection);
    QPushButton *deleteButton = new QPushButton("선택 상품 삭제");
    connect(deleteButton, SIGNAL(clicked()), SLOT(deleteProduct()));

    QVBoxLayout *listLayout = new QVBoxLayout;
    listLayout->addWidget(m_products);
    listLayout->addWidget(deleteButton, 0, Qt::AlignRight);

    QWidget *productTab = new QWidget;
    QHBoxLayout *productLayout = new QHBoxLayout(productTab);
    productLayout->addLayout(form);
    productLayout->addLayout(listLayout, 1);

    // ---- 매출 확인 탭 ----
    m_unit = new QComboBox;
    m_unit->addItem("일별", "day");
    m_unit->addItem("월별", "month");
    QPushButton *refresh = new QPushButton("조회");
    connect(m_unit, SIGNAL(currentIndexChanged(int)), SLOT(loadSales()));
    connect(refresh, SIGNAL(clicked()), SLOT(loadSales()));

    m_chart = new SalesChart;
    m_sales = makeTable(QStringList() << "기간" << "판매수량" << "카드" << "얼굴인식" << "매출 합계");
    m_salesTotal = new QLabel;
    m_salesTotal->setAlignment(Qt::AlignRight);
    m_salesTotal->setStyleSheet("font-size: 20px; font-weight: bold;");

    QHBoxLayout *salesBar = new QHBoxLayout;
    salesBar->addWidget(new QLabel("구분"));
    salesBar->addWidget(m_unit);
    salesBar->addWidget(refresh);
    salesBar->addStretch();

    QWidget *salesTab = new QWidget;
    QVBoxLayout *salesLayout = new QVBoxLayout(salesTab);
    // 위: 그래프, 아래: 상세 표 (경계선을 끌어서 크기 조절)
    QSplitter *splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(m_chart);
    splitter->addWidget(m_sales);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    salesLayout->addLayout(salesBar);
    salesLayout->addWidget(m_salesTotal);
    salesLayout->addWidget(splitter, 1);

    QTabWidget *tabs = new QTabWidget;
    tabs->addTab(productTab, "상품 관리");
    tabs->addTab(salesTab, "매출 확인");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(tabs);
}

void AdminWidget::start(const QVariantMap &user)
{
    m_welcome->setText(QString("관리자 모드 - %1").arg(user.value("name").toString()));
    clearForm();
    loadProducts();
    loadSales();
}

void AdminWidget::clearForm()
{
    m_name->clear();
    m_price->clear();
    m_imageData.clear();
    m_preview->setPixmap(QPixmap());
    m_preview->setText("이미지 없음");
}

void AdminWidget::chooseImage()
{
    const QString path = QFileDialog::getOpenFileName(this, "상품 이미지 선택", QString(),
                                                      "Images (*.png *.jpg *.jpeg *.bmp)");
    if (path.isEmpty())
        return;
    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, "이미지", "이미지를 열 수 없습니다.");
        return;
    }

    // 전송량을 줄이기 위해 300x300 이내로 줄여 PNG로 저장
    image = image.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageData.clear();
    QBuffer buffer(&m_imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    m_preview->setPixmap(QPixmap::fromImage(image).scaled(m_preview->size(), Qt::KeepAspectRatio));
}

void AdminWidget::addProduct()
{
    const QString name = m_name->text().trimmed();
    const int price = m_price->text().toInt();
    if (name.isEmpty() || price <= 0) {
        QMessageBox::warning(this, "상품 추가", "상품명과 가격을 입력하세요.");
        return;
    }

    QString err;
    if (!ServerClient::call("ADD_PRODUCT", QVariantList() << name << price << m_imageData, 0, &err)) {
        QMessageBox::warning(this, "상품 추가 실패", err);
        return;
    }
    QMessageBox::information(this, "상품 추가", QString("'%1' 상품이 추가되었습니다.").arg(name));
    clearForm();
    loadProducts();
}

void AdminWidget::deleteProduct()
{
    const int row = m_products->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "상품 삭제", "삭제할 상품을 목록에서 선택하세요.");
        return;
    }
    const int id = m_products->item(row, 0)->text().toInt();
    const QString name = m_products->item(row, 1)->text();
    if (QMessageBox::question(this, "상품 삭제",
                              QString("'%1' 상품을 삭제하시겠습니까?\n(이미 발생한 매출 기록은 유지됩니다)").arg(name),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QString err;
    if (!ServerClient::call("DELETE_PRODUCT", QVariantList() << id, 0, &err)) {
        QMessageBox::warning(this, "상품 삭제 실패", err);
        return;
    }
    loadProducts();
}

void AdminWidget::loadProducts()
{
    QVariant data;
    QString err;
    if (!ServerClient::call("PRODUCTS", QVariantList(), &data, &err)) {
        QMessageBox::warning(this, "상품 조회 실패", err);
        return;
    }
    const QVariantList list = data.toList();
    m_products->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        const QVariantMap p = list.at(i).toMap();
        m_products->setItem(i, 0, new QTableWidgetItem(p.value("id").toString()));
        m_products->setItem(i, 1, new QTableWidgetItem(p.value("name").toString()));
        m_products->setItem(i, 2, new QTableWidgetItem(won(p.value("price").toInt())));
    }
}

void AdminWidget::loadSales()
{
    QVariant data;
    QString err;
    const QString unit = m_unit->itemData(m_unit->currentIndex()).toString();
    if (!ServerClient::call("SALES", QVariantList() << unit, &data, &err)) {
        QMessageBox::warning(this, "매출 조회 실패", err);
        return;
    }

    const QVariantList list = data.toList();
    int sum = 0;
    m_sales->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        const QVariantMap s = list.at(i).toMap();
        m_sales->setItem(i, 0, new QTableWidgetItem(s.value("period").toString()));
        m_sales->setItem(i, 1, new QTableWidgetItem(QString("%L1").arg(s.value("qty").toInt())));
        m_sales->setItem(i, 2, new QTableWidgetItem(won(s.value("card").toInt())));
        m_sales->setItem(i, 3, new QTableWidgetItem(won(s.value("face").toInt())));
        m_sales->setItem(i, 4, new QTableWidgetItem(won(s.value("total").toInt())));
        sum += s.value("total").toInt();
    }
    m_salesTotal->setText("전체 매출: " + won(sum));

    if (unit == "month")
        m_chart->setData(list, 12, "월별 매출 (최근 12개월)");
    else
        m_chart->setData(list, 14, "일별 매출 (최근 14일)");
}
