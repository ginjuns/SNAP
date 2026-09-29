#include "adminwidget.h"
#include "saleschart.h"
#include "serverclient.h"
#include "virtualkeyboard.h"

#include "qtcompat.h"

static QTableWidget *makeTable(const QStringList &headers)
{
    QTableWidget *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    stretchColumns(table->horizontalHeader());
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->hide();
    return table;
}

static QTableWidgetItem *cell(const QString &text, bool number = false)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    if (number)
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
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
    m_name->setPlaceholderText("터치하여 입력");
    VirtualKeyboard::attach(m_name, VirtualKeyboard::Text, "상품명 입력");

    m_price = new QLineEdit;
    m_price->setValidator(new QIntValidator(1, 100000000, m_price));   // 숫자만 입력
    m_price->setPlaceholderText("터치하여 입력");
    VirtualKeyboard::attach(m_price, VirtualKeyboard::Number, "가격 입력 (원)");

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
    m_unit->addItem("일별 (날짜 선택)");
    m_unit->addItem("월별 (월 선택)");
    m_date = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);   // 터치로 달력에서 선택
    m_date->setDisplayFormat("yyyy-MM-dd");
    m_date->setMinimumWidth(170);
    QPushButton *prev = new QPushButton("◀ 이전");
    QPushButton *next = new QPushButton("다음 ▶");
    QPushButton *search = new QPushButton("검색");
    connect(m_unit, SIGNAL(currentIndexChanged(int)), SLOT(onUnitChanged()));
    connect(m_date, SIGNAL(dateChanged(QDate)), SLOT(loadSales()));
    connect(prev, SIGNAL(clicked()), SLOT(prevPeriod()));
    connect(next, SIGNAL(clicked()), SLOT(nextPeriod()));
    connect(search, SIGNAL(clicked()), SLOT(loadSales()));

    QHBoxLayout *salesBar = new QHBoxLayout;
    salesBar->addWidget(m_unit);
    salesBar->addWidget(prev);
    salesBar->addWidget(m_date);
    salesBar->addWidget(next);
    salesBar->addWidget(search);
    salesBar->addStretch();

    m_summary = new QLabel;
    m_summary->setStyleSheet("font-size: 18px; font-weight: bold;");

    m_chart = new SalesChart;
    m_details = makeTable(QStringList() << "일시" << "구매자" << "상품" << "수량" << "금액" << "결제수단");
    m_byBuyer = makeTable(QStringList() << "구매자" << "상품" << "수량" << "금액");

    QTabWidget *detailTabs = new QTabWidget;
    detailTabs->addTab(m_details, "구매 상세 내역");
    detailTabs->addTab(m_byBuyer, "구매자·상품별 합계");

    // 위: 그래프, 아래: 상세 표 (경계선을 끌어서 크기 조절)
    QSplitter *splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(m_chart);
    splitter->addWidget(detailTabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);

    QWidget *salesTab = new QWidget;
    QVBoxLayout *salesLayout = new QVBoxLayout(salesTab);
    salesLayout->addLayout(salesBar);
    salesLayout->addWidget(m_summary);
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

    m_date->blockSignals(true);   // 날짜 변경으로 인한 중복 조회 방지
    m_date->setDate(QDate::currentDate());
    m_date->blockSignals(false);
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

    QString err;   // 상품명 중복은 서버에서 확인
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
        m_products->setItem(i, 0, cell(p.value("id").toString()));
        m_products->setItem(i, 1, cell(p.value("name").toString()));
        m_products->setItem(i, 2, cell(won(p.value("price").toInt()), true));
    }
}

bool AdminWidget::isMonthly() const
{
    return m_unit->currentIndex() == 1;
}

void AdminWidget::onUnitChanged()
{
    m_date->setDisplayFormat(isMonthly() ? "yyyy년 MM월" : "yyyy-MM-dd");
    loadSales();
}

void AdminWidget::prevPeriod()
{
    m_date->setDate(isMonthly() ? m_date->date().addMonths(-1) : m_date->date().addDays(-1));
}

void AdminWidget::nextPeriod()
{
    m_date->setDate(isMonthly() ? m_date->date().addMonths(1) : m_date->date().addDays(1));
}

void AdminWidget::loadSales()
{
    const bool monthly = isMonthly();
    const QDate date = m_date->date();
    const QString key = date.toString(monthly ? "yyyy-MM" : "yyyy-MM-dd");

    QVariant data;
    QString err;
    if (!ServerClient::call("SALES", QVariantList() << (monthly ? "month" : "day") << key, &data, &err)) {
        QMessageBox::warning(this, "매출 조회 실패", err);
        return;
    }
    const QVariantMap result = data.toMap();

    // ---- 그래프: 매출이 없는 시간/날짜도 0으로 채워 전체 구간을 보여준다 ----
    const int slotCount = monthly ? date.daysInMonth() : 24;
    QVector<int> card(slotCount, 0), face(slotCount, 0);
    foreach (const QVariant &v, result.value("buckets").toList()) {
        const QVariantMap b = v.toMap();
        const int idx = b.value("bucket").toInt() - (monthly ? 1 : 0);   // 일: 1~31, 시: 0~23
        if (idx >= 0 && idx < slotCount) {
            card[idx] = b.value("card").toInt();
            face[idx] = b.value("face").toInt();
        }
    }
    QVariantList bars;
    int cardSum = 0, faceSum = 0;
    for (int i = 0; i < slotCount; ++i) {
        QVariantMap bar;
        const int n = monthly ? i + 1 : i;
        bar["label"] = QString(monthly ? "%1일" : "%1시").arg(n);
        bar["period"] = monthly ? QString("%1-%2").arg(key).arg(n, 2, 10, QChar('0'))
                                : QString("%1 %2시").arg(key).arg(n, 2, 10, QChar('0'));
        bar["card"] = card[i];
        bar["face"] = face[i];
        bars << bar;
        cardSum += card[i];
        faceSum += face[i];
    }
    m_chart->setData(bars, monthly ? date.toString("yyyy년 M월") + " 일자별 매출"
                                   : date.toString("yyyy년 M월 d일") + " 시간대별 매출");

    // ---- 구매 상세 내역 + 구매자·상품별 합계 ----
    const QVariantList details = result.value("details").toList();
    QMap<QString, QPair<int, int> > byBuyer;   // "구매자\t상품" -> (수량, 금액)
    int qtySum = 0;
    m_details->setRowCount(details.size());
    for (int i = 0; i < details.size(); ++i) {
        const QVariantMap d = details.at(i).toMap();
        const QString buyer = d.value("buyer").toString();
        const QString product = d.value("product").toString();
        const int qty = d.value("qty").toInt();
        const int amount = d.value("amount").toInt();
        const QString soldAt = d.value("soldAt").toString();

        m_details->setItem(i, 0, cell(monthly ? soldAt.mid(5, 11) : soldAt.mid(11)));   // MM-dd HH:mm / HH:mm:ss
        m_details->setItem(i, 1, cell(buyer));
        m_details->setItem(i, 2, cell(product));
        m_details->setItem(i, 3, cell(QString("%L1개").arg(qty), true));
        m_details->setItem(i, 4, cell(won(amount), true));
        m_details->setItem(i, 5, cell(d.value("method").toString() == "face" ? "얼굴인식" : "카드"));

        QPair<int, int> &sum = byBuyer[buyer + '\t' + product];
        sum.first += qty;
        sum.second += amount;
        qtySum += qty;
    }

    m_byBuyer->setRowCount(byBuyer.size());
    int row = 0;
    for (QMap<QString, QPair<int, int> >::const_iterator it = byBuyer.constBegin(); it != byBuyer.constEnd(); ++it, ++row) {
        const QStringList names = it.key().split('\t');
        m_byBuyer->setItem(row, 0, cell(names.value(0)));
        m_byBuyer->setItem(row, 1, cell(names.value(1)));
        m_byBuyer->setItem(row, 2, cell(QString("%L1개").arg(it.value().first), true));
        m_byBuyer->setItem(row, 3, cell(won(it.value().second), true));
    }

    m_summary->setText(QString("합계 %1   |   카드 %2   |   얼굴인식 %3   |   판매수량 %L4개")
                           .arg(won(cardSum + faceSum)).arg(won(cardSum)).arg(won(faceSum)).arg(qtySum));
}
