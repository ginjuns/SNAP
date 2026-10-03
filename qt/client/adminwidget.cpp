#include "adminwidget.h"
#include "ui_adminwidget.h"
#include "saleschart.h"
#include "serverclient.h"
#include "virtualkeyboard.h"

#include "kioskdialog.h"
#include "qtcompat.h"

static QTableWidgetItem *cell(const QString &text, bool number = false)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    if (number)
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

static const char *kNoImage = "사진 없음\n\n터치하여 추가";

enum IconType { PlusIcon, PencilIcon, ResetIcon, TrashIcon };

// 버튼 아이콘을 직접 그린다. (이미지 파일/추가 모듈 없이 어느 환경에서나 같은 모양)
static QIcon drawIcon(IconType type)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor color = (type == TrashIcon) ? QColor("#d32f2f") : (type == ResetIcon) ? QColor("#52514e")
                                                                                        : QColor("#2a78d6");
    p.setPen(QPen(color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    switch (type) {
    case PlusIcon:
        p.drawLine(32, 12, 32, 52);
        p.drawLine(12, 32, 52, 32);
        break;
    case PencilIcon: {   // 오른쪽 위 -> 왼쪽 아래로 기울어진 연필
        p.translate(34, 30);
        p.rotate(45);
        p.drawRect(QRectF(-6, -22, 12, 34));
        p.drawLine(QPointF(-6, -14), QPointF(6, -14));
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << QPointF(-6, 12) << QPointF(6, 12) << QPointF(0, 24));
        break;
    }
    case ResetIcon: {   // 시계 방향 회전 화살표: 위에서 시작해 왼쪽에서 끝남
        const QPointF c(32, 32);
        const qreal r = 18;
        p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 90 * 16, -270 * 16);
        const qreal e = M_PI;   // 끝나는 각도(180도, 왼쪽)
        const QPointF end(c.x() + r * qCos(e), c.y() - r * qSin(e));
        const QPointF dir(qSin(e), qCos(e));         // 시계 방향 진행 방향 (화면 좌표)
        const QPointF out(qCos(e), -qSin(e));        // 바깥쪽
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << end + dir * 10 << end + out * 8 << end - out * 8);
        break;
    }
    case TrashIcon:
        p.drawLine(12, 17, 52, 17);                                             // 뚜껑
        p.drawPolyline(QPolygonF() << QPointF(25, 17) << QPointF(25, 10) << QPointF(39, 10) << QPointF(39, 17));
        p.drawPolygon(QPolygonF() << QPointF(17, 23) << QPointF(47, 23) << QPointF(44, 55) << QPointF(20, 55));
        p.drawLine(28, 30, 28, 48);
        p.drawLine(36, 30, 36, 48);
        break;
    }
    return QIcon(pixmap);
}

static void setDrawnIcon(QPushButton *button, IconType type)
{
    button->setIcon(drawIcon(type));
    button->setIconSize(QSize(px(32), px(32)));
}

AdminWidget::AdminWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::AdminWidget), m_imageChanged(false)
{
    ui->setupUi(this);   // 화면 배치: adminwidget.ui
    scaleUi(this);
    connect(ui->logoutButton, SIGNAL(clicked()), SIGNAL(finished()));

    // ---- 상품 관리 탭 ----
    VirtualKeyboard::attach(ui->nameEdit, VirtualKeyboard::Text, "상품명 입력");
    ui->priceEdit->setValidator(new QIntValidator(1, 100000000, ui->priceEdit));   // 숫자만 입력
    VirtualKeyboard::attach(ui->priceEdit, VirtualKeyboard::Number, "가격 입력 (원)");
    ui->stockEdit->setValidator(new QIntValidator(0, 1000000, ui->stockEdit));
    VirtualKeyboard::attach(ui->stockEdit, VirtualKeyboard::Number, "재고 수량 입력 (개)");

    // 사진 영역을 터치하면 사진 추가/변경
    ui->previewLabel->setText(kNoImage);
    ui->previewLabel->installEventFilter(this);

    // 아이콘 버튼: 추가 / 수정 / 새로 입력 / 삭제
    setDrawnIcon(ui->addButton, PlusIcon);
    setDrawnIcon(ui->updateButton, PencilIcon);
    setDrawnIcon(ui->clearButton, ResetIcon);
    setDrawnIcon(ui->deleteButton, TrashIcon);
    connect(ui->addButton, SIGNAL(clicked()), SLOT(addProduct()));
    connect(ui->updateButton, SIGNAL(clicked()), SLOT(updateProduct()));
    connect(ui->clearButton, SIGNAL(clicked()), SLOT(clearForm()));
    connect(ui->deleteButton, SIGNAL(clicked()), SLOT(deleteProduct()));

    // 목록에서 상품을 누르면 위쪽 입력칸에 채워지고, [수정]으로 반영
    stretchColumns(ui->productTable->horizontalHeader());
    connect(ui->productTable, SIGNAL(itemSelectionChanged()), SLOT(onProductSelected()));

    // ---- 매출 확인 탭 ----
    // 세로 화면은 폭이 좁아 두 줄로: [일별/월별 ... 오늘] / [◀ 이전  날짜  다음 ▶]
    ui->dateEdit->setDate(QDate::currentDate());   // 터치로 달력에서 선택
    connect(ui->unitCombo, SIGNAL(currentIndexChanged(int)), SLOT(onUnitChanged()));
    connect(ui->dateEdit, SIGNAL(dateChanged(QDate)), SLOT(loadSales()));
    connect(ui->prevButton, SIGNAL(clicked()), SLOT(prevPeriod()));
    connect(ui->nextButton, SIGNAL(clicked()), SLOT(nextPeriod()));
    connect(ui->todayButton, SIGNAL(clicked()), SLOT(goToday()));

    // 위: 그래프, 아래: 상세 표 (경계선을 끌어서 크기 조절)
    stretchColumns(ui->detailTable->horizontalHeader());
    stretchColumns(ui->byBuyerTable->horizontalHeader());
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 1);
}

AdminWidget::~AdminWidget()
{
    delete ui;
}

void AdminWidget::start(const QVariantMap &user)
{
    ui->welcomeLabel->setText(QString("관리자 모드 - %1").arg(user.value("name").toString()));
    clearForm();
    loadProducts();

    ui->dateEdit->blockSignals(true);   // 날짜 변경으로 인한 중복 조회 방지
    ui->dateEdit->setDate(QDate::currentDate());
    ui->dateEdit->blockSignals(false);
    loadSales();
}

void AdminWidget::clearForm()
{
    ui->productTable->clearSelection();
    ui->nameEdit->clear();
    ui->priceEdit->clear();
    ui->stockEdit->clear();
    m_imageData.clear();
    m_imageChanged = false;
    ui->previewLabel->setPixmap(QPixmap());
    ui->previewLabel->setText(kNoImage);
}

bool AdminWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->previewLabel && event->type() == QEvent::MouseButtonRelease) {
        chooseImage();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void AdminWidget::chooseImage()
{
    // Ubuntu 기본 파일 창은 별도 프로그램 창이라 상단바/독이 보이므로,
    // Qt가 직접 그리는 파일 창을 키오스크 화면 안에 띄운다. (처음 위치: 사진 폴더)
    QFileDialog dialog(this, "상품 이미지 선택",
                       QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                       "이미지 (*.png *.jpg *.jpeg *.bmp)");
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setViewMode(QFileDialog::List);
    dialog.resize(px(680), px(900));
    if (execInWindow(&dialog) != QDialog::Accepted || dialog.selectedFiles().isEmpty())
        return;
    const QString path = dialog.selectedFiles().first();
    QImage image(path);
    if (image.isNull()) {
        KioskMessage::warning(this, "이미지", "이미지를 열 수 없습니다.");
        return;
    }

    // 전송량을 줄이기 위해 300x300 이내로 줄여 PNG로 저장
    image = image.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_imageData.clear();
    QBuffer buffer(&m_imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    m_imageChanged = true;

    ui->previewLabel->setPixmap(QPixmap::fromImage(image).scaled(ui->previewLabel->size(), Qt::KeepAspectRatio));
}

int AdminWidget::selectedRow() const
{
    const QList<QTableWidgetItem *> items = ui->productTable->selectedItems();
    return items.isEmpty() ? -1 : items.first()->row();
}

// 입력칸 값 확인. 잘못됐으면 안내 후 false
bool AdminWidget::readForm(QString *name, int *price, int *stock)
{
    *name = ui->nameEdit->text().trimmed();
    *price = ui->priceEdit->text().toInt();
    *stock = ui->stockEdit->text().toInt();
    if (name->isEmpty() || *price <= 0 || ui->stockEdit->text().isEmpty()) {
        KioskMessage::warning(this, "상품", "상품명, 가격, 재고를 모두 입력하세요.");
        return false;
    }
    return true;
}

void AdminWidget::addProduct()
{
    QString name;
    int price, stock;
    if (!readForm(&name, &price, &stock))
        return;

    QString err;   // 상품명 중복은 서버에서 확인
    const QVariantMap args{{"name", name}, {"price", price}, {"stock", stock},
                           {"image", QString::fromLatin1(m_imageData.toBase64())}};
    if (!ServerClient::call("ADD_PRODUCT", args, 0, &err)) {
        KioskMessage::warning(this, "상품 추가 실패", err);
        return;
    }
    KioskMessage::information(this, "상품 추가", QString("'%1' 상품이 추가되었습니다.").arg(name));
    clearForm();
    loadProducts();
}

void AdminWidget::updateProduct()
{
    const int row = selectedRow();
    if (row < 0) {
        KioskMessage::information(this, "상품 수정", "수정할 상품을 목록에서 선택하세요.");
        return;
    }
    QString name;
    int price, stock;
    if (!readForm(&name, &price, &stock))
        return;

    QVariantMap args{{"productId", m_productList.value(row).toMap().value("id").toInt()},
                     {"name", name}, {"price", price}, {"stock", stock}};
    if (m_imageChanged)   // 사진을 새로 고른 경우에만 교체
        args["image"] = QString::fromLatin1(m_imageData.toBase64());

    QString err;
    if (!ServerClient::call("UPDATE_PRODUCT", args, 0, &err)) {
        KioskMessage::warning(this, "상품 수정 실패", err);
        return;
    }
    KioskMessage::information(this, "상품 수정", QString("'%1' 상품이 수정되었습니다.").arg(name));
    clearForm();
    loadProducts();
}

void AdminWidget::deleteProduct()
{
    const int row = selectedRow();
    if (row < 0) {
        KioskMessage::information(this, "상품 삭제", "삭제할 상품을 목록에서 선택하세요.");
        return;
    }
    const QVariantMap p = m_productList.value(row).toMap();
    if (KioskMessage::question(this, "상품 삭제",
                              QString("'%1' 상품을 삭제하시겠습니까?\n(이미 발생한 매출 기록은 유지됩니다)")
                                  .arg(p.value("name").toString()),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QString err;
    if (!ServerClient::call("DELETE_PRODUCT", QVariantMap{{"productId", p.value("id").toInt()}}, 0, &err)) {
        KioskMessage::warning(this, "상품 삭제 실패", err);
        return;
    }
    clearForm();
    loadProducts();
}

// 목록에서 고른 상품을 입력칸에 채운다.
void AdminWidget::onProductSelected()
{
    const int row = selectedRow();
    if (row < 0)
        return;
    const QVariantMap p = m_productList.value(row).toMap();
    ui->nameEdit->setText(p.value("name").toString());
    ui->priceEdit->setText(QString::number(p.value("price").toInt()));
    ui->stockEdit->setText(QString::number(p.value("stock").toInt()));

    m_imageData = QByteArray::fromBase64(p.value("image").toString().toLatin1());
    m_imageChanged = false;
    QPixmap pixmap;
    if (pixmap.loadFromData(m_imageData)) {
        ui->previewLabel->setPixmap(pixmap.scaled(ui->previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        ui->previewLabel->setPixmap(QPixmap());
        ui->previewLabel->setText(kNoImage);
    }
}

void AdminWidget::loadProducts()
{
    QVariant data;
    QString err;
    if (!ServerClient::call("PRODUCTS", QVariantMap(), &data, &err)) {
        KioskMessage::warning(this, "상품 조회 실패", err);
        return;
    }
    m_productList = data.toList();
    ui->productTable->clearSelection();
    ui->productTable->setRowCount(m_productList.size());
    for (int i = 0; i < m_productList.size(); ++i) {
        const QVariantMap p = m_productList.at(i).toMap();
        const int stock = p.value("stock").toInt();
        ui->productTable->setItem(i, 0, cell(p.value("name").toString()));
        ui->productTable->setItem(i, 1, cell(stock > 0 ? QString("%L1개").arg(stock) : QString("품절"), true));
        ui->productTable->setItem(i, 2, cell(won(p.value("price").toInt()), true));
    }
}

bool AdminWidget::isMonthly() const
{
    return ui->unitCombo->currentIndex() == 1;
}

void AdminWidget::onUnitChanged()
{
    ui->dateEdit->setDisplayFormat(isMonthly() ? "yyyy년 MM월" : "yyyy-MM-dd");
    loadSales();
}

void AdminWidget::prevPeriod()
{
    ui->dateEdit->setDate(isMonthly() ? ui->dateEdit->date().addMonths(-1) : ui->dateEdit->date().addDays(-1));
}

void AdminWidget::nextPeriod()
{
    ui->dateEdit->setDate(isMonthly() ? ui->dateEdit->date().addMonths(1) : ui->dateEdit->date().addDays(1));
}

void AdminWidget::loadSales()
{
    const bool monthly = isMonthly();
    const QDate date = ui->dateEdit->date();
    const QString key = date.toString(monthly ? "yyyy-MM" : "yyyy-MM-dd");

    QVariant data;
    QString err;
    const QVariantMap args{{"unit", monthly ? "month" : "day"}, {"date", key}};
    if (!ServerClient::call("SALES", args, &data, &err)) {
        KioskMessage::warning(this, "매출 조회 실패", err);
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
    ui->chart->setData(bars, monthly ? date.toString("yyyy년 M월") + " 일자별 매출"
                                   : date.toString("yyyy년 M월 d일") + " 시간대별 매출");

    // ---- 구매 상세 내역 + 구매자·상품별 합계 ----
    const QVariantList details = result.value("details").toList();
    QMap<QString, QPair<int, int> > byBuyer;   // "구매자\t상품" -> (수량, 금액)
    int qtySum = 0;
    ui->detailTable->setRowCount(details.size());
    for (int i = 0; i < details.size(); ++i) {
        const QVariantMap d = details.at(i).toMap();
        const QString buyer = d.value("buyer").toString();
        const QString product = d.value("product").toString();
        const int qty = d.value("qty").toInt();
        const int amount = d.value("amount").toInt();
        const QString soldAt = d.value("soldAt").toString();

        ui->detailTable->setItem(i, 0, cell(monthly ? soldAt.mid(5, 11) : soldAt.mid(11)));   // MM-dd HH:mm / HH:mm:ss
        ui->detailTable->setItem(i, 1, cell(buyer));
        ui->detailTable->setItem(i, 2, cell(product));
        ui->detailTable->setItem(i, 3, cell(QString("%L1개").arg(qty), true));
        ui->detailTable->setItem(i, 4, cell(won(amount), true));
        ui->detailTable->setItem(i, 5, cell(d.value("method").toString() == "face" ? "얼굴인식" : "카드"));

        QPair<int, int> &sum = byBuyer[buyer + '\t' + product];
        sum.first += qty;
        sum.second += amount;
        qtySum += qty;
    }

    ui->byBuyerTable->setRowCount(byBuyer.size());
    int row = 0;
    for (QMap<QString, QPair<int, int> >::const_iterator it = byBuyer.constBegin(); it != byBuyer.constEnd(); ++it, ++row) {
        const QStringList names = it.key().split('\t');
        ui->byBuyerTable->setItem(row, 0, cell(names.value(0)));
        ui->byBuyerTable->setItem(row, 1, cell(names.value(1)));
        ui->byBuyerTable->setItem(row, 2, cell(QString("%L1개").arg(it.value().first), true));
        ui->byBuyerTable->setItem(row, 3, cell(won(it.value().second), true));
    }

    ui->summaryLabel->setText(QString("합계 %1   |   카드 %2   |   얼굴인식 %3   |   판매수량 %L4개")
                           .arg(won(cardSum + faceSum)).arg(won(cardSum)).arg(won(faceSum)).arg(qtySum));
}

void AdminWidget::goToday()
{
    if (ui->dateEdit->date() == QDate::currentDate())
        loadSales();   // 이미 오늘이면 새로 조회만
    else
        ui->dateEdit->setDate(QDate::currentDate());
}
