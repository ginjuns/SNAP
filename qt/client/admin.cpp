#include "admin.h"
#include "ui_admin.h"
#include "calendar.h"
#include "chart.h"
#include "keyboard.h"
#include "net.h"
#include "packet.h"
#include "server.h"
#include "popup.h"
#include "util.h"

static QTableWidgetItem *cell(const QString &text, bool num = false)
{
    QTableWidgetItem *c = new QTableWidgetItem(text);
    if (num)
        c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return c;
}

static const char *NO_IMAGE = "사진 없음\n\n터치하여 추가";

enum Icon { Plus, Pencil, Reset, Trash };

static QIcon makeIcon(Icon type)
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QColor color = (type == Trash) ? QColor("#d32f2f") : (type == Reset) ? QColor("#52514e") : QColor("#2a78d6");
    p.setPen(QPen(color, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    switch (type) {
    case Plus:
        p.drawLine(32, 12, 32, 52);
        p.drawLine(12, 32, 52, 32);
        break;
    case Pencil:
        p.translate(34, 30);
        p.rotate(45);
        p.drawRect(QRectF(-6, -22, 12, 34));
        p.drawLine(QPointF(-6, -14), QPointF(6, -14));
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << QPointF(-6, 12) << QPointF(6, 12) << QPointF(0, 24));
        break;
    case Reset: {
        QPointF c(32, 32);
        qreal r = 18;
        p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 90 * 16, -270 * 16);
        qreal e = M_PI;
        QPointF end(c.x() + r * qCos(e), c.y() - r * qSin(e));
        QPointF dir(qSin(e), qCos(e));
        QPointF out(qCos(e), -qSin(e));
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << end + dir * 10 << end + out * 8 << end - out * 8);
        break;
    }
    case Trash:
        p.drawLine(12, 17, 52, 17);
        p.drawPolyline(QPolygonF() << QPointF(25, 17) << QPointF(25, 10) << QPointF(39, 10) << QPointF(39, 17));
        p.drawPolygon(QPolygonF() << QPointF(17, 23) << QPointF(47, 23) << QPointF(44, 55) << QPointF(20, 55));
        p.drawLine(28, 30, 28, 48);
        p.drawLine(36, 30, 36, 48);
        break;
    }
    return QIcon(pix);
}

static void setIcon(QPushButton *btn, Icon type)
{
    btn->setIcon(makeIcon(type));
    btn->setIconSize(QSize(px(32), px(32)));
}

Admin::Admin(QWidget *parent)
    : QWidget(parent), ui(new Ui::Admin), m_imgChanged(false)
{
    ui->setupUi(this);
    scaleUi(this);
    connect(ui->btnLogout, SIGNAL(clicked()), SIGNAL(done()));

    Keyboard::attach(ui->editName, Keyboard::Text, "상품명 입력");
    ui->editPrice->setValidator(new QIntValidator(1, 100000000, ui->editPrice));
    Keyboard::attach(ui->editPrice, Keyboard::Number, "가격 입력 (원)");
    ui->editStock->setValidator(new QIntValidator(0, 1000000, ui->editStock));
    Keyboard::attach(ui->editStock, Keyboard::Number, "재고 수량 입력 (개)");

    ui->comboShelf->addItem("선택 안 함");
    for (int i = 1; i <= SHELF_COUNT; ++i)
        ui->comboShelf->addItem(QString("%1번").arg(i));
    ui->comboShelf->setView(new QListView);
    ui->comboShelf->setStyleSheet(css("QComboBox QAbstractItemView::item { min-height: 48px; }"));

    ui->lblImage->setText(NO_IMAGE);
    ui->lblImage->installEventFilter(this);

    setIcon(ui->btnAdd, Plus);
    setIcon(ui->btnEdit, Pencil);
    setIcon(ui->btnReset, Reset);
    setIcon(ui->btnDel, Trash);
    connect(ui->btnAdd, SIGNAL(clicked()), SLOT(add()));
    connect(ui->btnEdit, SIGNAL(clicked()), SLOT(edit()));
    connect(ui->btnReset, SIGNAL(clicked()), SLOT(reset()));
    connect(ui->btnDel, SIGNAL(clicked()), SLOT(del()));

    stretch(ui->tableProduct);
    touchScroll(ui->tableProduct);
    connect(ui->tableProduct, SIGNAL(itemSelectionChanged()), SLOT(onSelect()));

    ui->date->setDate(QDate::currentDate());
    ui->date->findChild<QLineEdit *>()->installEventFilter(this);
    ui->comboUnit->setView(new QListView);
    ui->comboUnit->setStyleSheet(css("QComboBox QAbstractItemView::item { min-height: 48px; }"));
    connect(ui->comboUnit, SIGNAL(currentIndexChanged(int)), SLOT(onUnit()));
    connect(ui->date, SIGNAL(dateChanged(QDate)), SLOT(loadSales()));
    connect(ui->btnPrev, SIGNAL(clicked()), SLOT(prev()));
    connect(ui->btnNext, SIGNAL(clicked()), SLOT(next()));
    connect(ui->btnToday, SIGNAL(clicked()), SLOT(today()));

    stretch(ui->tableDetail);
    stretch(ui->tableBuyer);
    touchScroll(ui->tableDetail);
    touchScroll(ui->tableBuyer);
    ui->splitter->setHandleWidth(px(16));
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 1);

    connect(ui->btnReport, SIGNAL(clicked()), SLOT(makeReport()));
    connect(ui->comboCategory, SIGNAL(currentIndexChanged(int)), SLOT(onCategory()));
    connect(ui->btnForecast, SIGNAL(clicked()), SLOT(forecast()));
    foreach (QComboBox *c, QList<QComboBox *>() << ui->comboPeriod << ui->comboCategory << ui->comboTarget
                                                << ui->comboProduct << ui->comboDays) {
        c->setView(new QListView);
        c->setStyleSheet(css("QComboBox QAbstractItemView::item { min-height: 48px; }"));
    }
    touchScroll(ui->scrollChat);
    touchScroll(ui->tableForecast);
    stretch(ui->tableForecast);
}

Admin::~Admin()
{
    delete ui;
}

void Admin::start(const QVariantMap &user)
{
    ui->lblWelcome->setText(QString("관리자 모드 - %1").arg(user.value("name").toString()));
    reset();
    loadProducts();

    ui->date->blockSignals(true);
    ui->date->setDate(QDate::currentDate());
    ui->date->blockSignals(false);
    loadSales();
    loadTargets();
    onCategory();
}

void Admin::reset()
{
    ui->tableProduct->clearSelection();
    ui->editName->clear();
    ui->editPrice->clear();
    ui->editStock->clear();
    ui->comboShelf->setCurrentIndex(0);
    m_img.clear();
    m_imgChanged = false;
    ui->lblImage->setPixmap(QPixmap());
    ui->lblImage->setText(NO_IMAGE);
}

bool Admin::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == ui->lblImage && e->type() == QEvent::MouseButtonRelease) {
        pickImage();
        return true;
    }
    if (obj->parent() == ui->date) {
        if (e->type() == QEvent::MouseButtonRelease)
            pickDate();
        if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease
            || e->type() == QEvent::MouseButtonDblClick)
            return true;
    }
    return QWidget::eventFilter(obj, e);
}

void Admin::pickDate()
{
    Calendar dlg(ui->date->date(), monthly(), this);
    if (popup(&dlg) == QDialog::Accepted)
        ui->date->setDate(dlg.date());
}

void Admin::pickImage()
{
    QFileDialog dlg(this, "상품 이미지 선택",
                    QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                    "이미지 (*.png *.jpg *.jpeg *.bmp)");
    dlg.setOption(QFileDialog::DontUseNativeDialog);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setViewMode(QFileDialog::List);
    dlg.resize(px(680), px(900));
    foreach (QAbstractItemView *v, dlg.findChildren<QAbstractItemView *>())
        touchScroll(v);
    if (popup(&dlg) != QDialog::Accepted || dlg.selectedFiles().isEmpty())
        return;
    QImage img(dlg.selectedFiles().first());
    if (img.isNull()) {
        Msg::warn(this, "이미지", "이미지를 열 수 없습니다.");
        return;
    }

    img = img.scaled(300, 300, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_img.clear();
    QBuffer buf(&m_img);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    m_imgChanged = true;

    ui->lblImage->setPixmap(QPixmap::fromImage(img).scaled(ui->lblImage->size(), Qt::KeepAspectRatio));
}

int Admin::row() const
{
    QList<QTableWidgetItem *> items = ui->tableProduct->selectedItems();
    return items.isEmpty() ? -1 : items.first()->row();
}

bool Admin::readForm(QString *name, int *price, int *stock)
{
    *name = ui->editName->text().trimmed();
    *price = ui->editPrice->text().toInt();
    *stock = ui->editStock->text().toInt();
    if (name->isEmpty() || *price <= 0 || ui->editStock->text().isEmpty()) {
        Msg::warn(this, "상품", "상품명, 가격, 재고를 모두 입력하세요.");
        return false;
    }
    return true;
}

void Admin::add()
{
    QString name;
    int price, stock;
    if (!readForm(&name, &price, &stock))
        return;

    QString err;
    QVariantMap args{{"name", name}, {"price", price}, {"stock", stock},
                     {"shelf", ui->comboShelf->currentIndex()},
                     {"image", QString::fromLatin1(m_img.toBase64())}};
    if (!Net::call("ADD_PRODUCT", args, 0, &err)) {
        Msg::warn(this, "상품 추가 실패", err);
        return;
    }
    Msg::info(this, "상품 추가", QString("'%1' 상품이 추가되었습니다.").arg(name));
    reset();
    loadProducts();
}

void Admin::edit()
{
    int r = row();
    if (r < 0) {
        Msg::info(this, "상품 수정", "수정할 상품을 목록에서 선택하세요.");
        return;
    }
    QString name;
    int price, stock;
    if (!readForm(&name, &price, &stock))
        return;

    QVariantMap args{{"productId", m_list.value(r).toMap().value("id").toInt()},
                     {"name", name}, {"price", price}, {"stock", stock},
                     {"shelf", ui->comboShelf->currentIndex()}};
    if (m_imgChanged)
        args["image"] = QString::fromLatin1(m_img.toBase64());

    QString err;
    if (!Net::call("UPDATE_PRODUCT", args, 0, &err)) {
        Msg::warn(this, "상품 수정 실패", err);
        return;
    }
    Msg::info(this, "상품 수정", QString("'%1' 상품이 수정되었습니다.").arg(name));
    reset();
    loadProducts();
}

void Admin::del()
{
    int r = row();
    if (r < 0) {
        Msg::info(this, "상품 삭제", "삭제할 상품을 목록에서 선택하세요.");
        return;
    }
    QVariantMap p = m_list.value(r).toMap();
    if (!Msg::ask(this, "상품 삭제",
                  QString("'%1' 상품을 삭제하시겠습니까?\n(이미 발생한 매출 기록은 유지됩니다)")
                      .arg(p.value("name").toString())))
        return;

    QString err;
    if (!Net::call("DELETE_PRODUCT", QVariantMap{{"productId", p.value("id").toInt()}}, 0, &err)) {
        Msg::warn(this, "상품 삭제 실패", err);
        return;
    }
    reset();
    loadProducts();
}

void Admin::onSelect()
{
    int r = row();
    if (r < 0)
        return;
    QVariantMap p = m_list.value(r).toMap();
    ui->editName->setText(p.value("name").toString());
    ui->editPrice->setText(QString::number(p.value("price").toInt()));
    ui->editStock->setText(QString::number(p.value("stock").toInt()));
    ui->comboShelf->setCurrentIndex(p.value("shelf").toInt());

    m_img = QByteArray::fromBase64(p.value("image").toString().toLatin1());
    m_imgChanged = false;
    QPixmap pix;
    if (pix.loadFromData(m_img)) {
        ui->lblImage->setPixmap(pix.scaled(ui->lblImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        ui->lblImage->setPixmap(QPixmap());
        ui->lblImage->setText(NO_IMAGE);
    }
}

void Admin::loadProducts()
{
    QVariant data;
    QString err;
    if (!Net::call("PRODUCTS", QVariantMap(), &data, &err)) {
        Msg::warn(this, "상품 조회 실패", err);
        return;
    }
    m_list = data.toList();
    ui->tableProduct->clearSelection();
    ui->tableProduct->setRowCount(m_list.size());
    for (int i = 0; i < m_list.size(); ++i) {
        QVariantMap p = m_list.at(i).toMap();
        int stock = p.value("stock").toInt();
        ui->tableProduct->setItem(i, 0, cell(p.value("name").toString()));
        ui->tableProduct->setItem(i, 1, cell(stock > 0 ? QString("%L1개").arg(stock) : QString("품절"), true));
        ui->tableProduct->setItem(i, 2, cell(won(p.value("price").toInt()), true));
        int shelf = p.value("shelf").toInt();
        QString s = shelf ? QString("%1번").arg(shelf) : QString("-");
        if (Esp::shelfEmpty(shelf))
            s += " (비어 있음)";
        ui->tableProduct->setItem(i, 3, cell(s));
    }
}

bool Admin::monthly() const
{
    return ui->comboUnit->currentIndex() == 1;
}

void Admin::onUnit()
{
    ui->date->setDisplayFormat(monthly() ? "yyyy년 MM월" : "yyyy-MM-dd");
    loadSales();
}

void Admin::prev()
{
    ui->date->setDate(monthly() ? ui->date->date().addMonths(-1) : ui->date->date().addDays(-1));
}

void Admin::next()
{
    ui->date->setDate(monthly() ? ui->date->date().addMonths(1) : ui->date->date().addDays(1));
}

void Admin::loadSales()
{
    bool mon = monthly();
    QDate d = ui->date->date();
    QString key = d.toString(mon ? "yyyy-MM" : "yyyy-MM-dd");

    QVariant data;
    QString err;
    QVariantMap args{{"unit", mon ? "month" : "day"}, {"date", key}};
    if (!Net::call("SALES", args, &data, &err)) {
        Msg::warn(this, "매출 조회 실패", err);
        return;
    }
    QVariantMap res = data.toMap();

    int n = mon ? d.daysInMonth() : 24;
    QVector<int> card(n, 0), face(n, 0);
    foreach (const QVariant &v, res.value("buckets").toList()) {
        QVariantMap b = v.toMap();
        int i = b.value("bucket").toInt() - (mon ? 1 : 0);
        if (i >= 0 && i < n) {
            card[i] = b.value("card").toInt();
            face[i] = b.value("face").toInt();
        }
    }
    QVariantList bars;
    int cardSum = 0, faceSum = 0;
    for (int i = 0; i < n; ++i) {
        QVariantMap bar;
        int k = mon ? i + 1 : i;
        bar["label"] = QString(mon ? "%1일" : "%1시").arg(k);
        bar["period"] = mon ? QString("%1-%2").arg(key).arg(k, 2, 10, QChar('0'))
                            : QString("%1 %2시").arg(key).arg(k, 2, 10, QChar('0'));
        bar["card"] = card[i];
        bar["face"] = face[i];
        bars << bar;
        cardSum += card[i];
        faceSum += face[i];
    }
    ui->chart->setData(bars, mon ? d.toString("yyyy년 M월") + " 일자별 매출"
                                 : d.toString("yyyy년 M월 d일") + " 시간대별 매출");

    QVariantList list = res.value("details").toList();
    QMap<QString, QPair<int, int> > sum;
    int qtySum = 0;
    ui->tableDetail->setRowCount(list.size());
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap m = list.at(i).toMap();
        QString buyer = m.value("buyer").toString();
        QString product = m.value("product").toString();
        int qty = m.value("qty").toInt();
        int amount = m.value("amount").toInt();
        QString when = m.value("soldAt").toString();

        ui->tableDetail->setItem(i, 0, cell(mon ? when.mid(5, 11) : when.mid(11)));
        ui->tableDetail->setItem(i, 1, cell(buyer));
        ui->tableDetail->setItem(i, 2, cell(product));
        ui->tableDetail->setItem(i, 3, cell(QString("%L1개").arg(qty), true));
        ui->tableDetail->setItem(i, 4, cell(won(amount), true));
        ui->tableDetail->setItem(i, 5, cell(m.value("method").toString() == "face" ? "얼굴인식" : "카드"));

        QPair<int, int> &s = sum[buyer + '\t' + product];
        s.first += qty;
        s.second += amount;
        qtySum += qty;
    }

    ui->tableBuyer->setRowCount(sum.size());
    int r = 0;
    for (QMap<QString, QPair<int, int> >::const_iterator it = sum.constBegin(); it != sum.constEnd(); ++it, ++r) {
        QStringList names = it.key().split('\t');
        ui->tableBuyer->setItem(r, 0, cell(names.value(0)));
        ui->tableBuyer->setItem(r, 1, cell(names.value(1)));
        ui->tableBuyer->setItem(r, 2, cell(QString("%L1개").arg(it.value().first), true));
        ui->tableBuyer->setItem(r, 3, cell(won(it.value().second), true));
    }

    ui->lblSum->setText(QString("합계 %1   |   카드 %2   |   얼굴인식 %3   |   판매수량 %L4개")
                            .arg(won(cardSum + faceSum)).arg(won(cardSum)).arg(won(faceSum)).arg(qtySum));
}

void Admin::today()
{
    if (ui->date->date() == QDate::currentDate())
        loadSales();
    else
        ui->date->setDate(QDate::currentDate());
}

struct Question
{
    const char *category;
    const char *text;
    int target;
};

static const Question QUESTIONS[] = {
    {"매출", "오늘 매출 요약해줘", 0},
    {"매출", "지난주와 비교해줘", 0},
    {"매출", "매출이 떨어진 이유는?", 0},
    {"상품", "가장 잘 팔리는 상품은?", 0},
    {"상품", "안 팔리는 상품 개선 방법은?", 0},
    {"상품", "[상품] 판매 추이는?", 1},
    {"재고", "곧 품절될 상품은?", 0},
    {"재고", "이번 주 발주 추천해줘", 0},
    {"고객", "단골 고객 분석해줘", 0},
    {"고객", "[회원] 님의 구매 성향은?", 2},
    {"고객", "최근 안 오는 고객은?", 0},
    {"전략", "세트 메뉴 추천해줘", 0},
    {"전략", "바쁜 시간대 대비 방법은?", 0},
};

static void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();   // 누른 버튼 자신을 지울 수 있으므로 deleteLater
        delete item;
    }
}

void Admin::loadTargets()
{
    QVariant data;
    QString err;
    if (Net::call("MEMBERS", QVariantMap(), &data, &err))
        m_members = data.toList();

    ui->comboProduct->clear();
    ui->comboProduct->addItem("전체 상품", 0);
    foreach (const QVariant &v, m_list) {
        QVariantMap m = v.toMap();
        ui->comboProduct->addItem(m.value("name").toString(), m.value("id"));
    }
}

void Admin::makeReport()
{
    static const char *PERIODS[] = {"day", "week", "month"};

    ui->btnReport->setEnabled(false);
    ui->lblReportInfo->setText("AI가 분석 중입니다...");
    qApp->processEvents();                    // 위 문구가 먼저 화면에 보이도록

    QVariant data;
    QString err;
    QVariantMap args{{"period", PERIODS[ui->comboPeriod->currentIndex()]}};
    bool ok = Net::call("AI_REPORT", args, &data, &err);
    ui->btnReport->setEnabled(true);
    if (!ok) {
        ui->lblReportInfo->setText("리포트 생성 실패");
        Msg::warn(this, "AI 리포트", err);
        return;
    }
    ui->textReport->setHtml(data.toString());   // Qt 5.9에는 setMarkdown이 없어서 HTML로 받는다
    ui->lblReportInfo->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm 생성"));
}

void Admin::onCategory()
{
    clearLayout(ui->layoutQuestions);
    QString category = ui->comboCategory->currentText();
    int target = 0, n = 0;
    for (const Question &q : QUESTIONS) {
        if (category != q.category)
            continue;
        QPushButton *btn = new QPushButton(q.text);
        btn->setMinimumHeight(px(56));
        btn->setProperty("target", q.target);
        connect(btn, SIGNAL(clicked()), SLOT(ask()));
        ui->layoutQuestions->addWidget(btn, n / 2, n % 2);
        ++n;
        target = qMax(target, q.target);
    }

    ui->comboTarget->clear();
    foreach (const QVariant &v, target == 1 ? m_list : target == 2 ? m_members : QVariantList()) {
        QVariantMap m = v.toMap();
        ui->comboTarget->addItem(m.value("name").toString(), m.value("id"));
    }
    ui->comboTarget->setVisible(target != 0);
}

void Admin::ask()
{
    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (!btn)
        return;
    QString text = btn->text();
    int target = btn->property("target").toInt();

    QVariantMap args;
    if (target) {
        text.replace(target == 1 ? "[상품]" : "[회원]", ui->comboTarget->currentText());
        args[target == 1 ? "productId" : "userId"] = ui->comboTarget->currentData();
    }
    args["question"] = text;

    addBubble(text, true);
    QLabel *wait = addBubble("AI가 분석 중...", false);
    qApp->processEvents();

    QVariant data;
    QString err;
    bool ok = Net::call("AI_ASK", args, &data, &err);
    delete wait;
    if (!ok) {
        addBubble("오류: " + err, false);
        return;
    }
    QVariantMap res = data.toMap();
    addBubble(res.value("answer").toString(), false);
    setFollowUps(res.value("followUps").toStringList());
}

QLabel *Admin::addBubble(const QString &text, bool mine)
{
    QLabel *lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setMaximumWidth(ui->scrollChat->viewport()->width() * 3 / 4);
    lbl->setStyleSheet(css(mine ? "background: #2a78d6; color: white; border-radius: 12px; padding: 10px; font-size: 17px;"
                                : "background: #eeede8; color: #0b0b0b; border-radius: 12px; padding: 10px; font-size: 17px;"));
    ui->layoutChat->addWidget(lbl, 0, mine ? Qt::AlignRight : Qt::AlignLeft);

    QTimer::singleShot(50, this, [this]() {   // 크기 계산이 끝난 뒤 맨 아래로 스크롤
        QScrollBar *bar = ui->scrollChat->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
    return lbl;
}

void Admin::setFollowUps(const QStringList &list)
{
    clearLayout(ui->layoutFollow);
    foreach (const QString &s, list) {
        QPushButton *btn = new QPushButton(s);
        btn->setMinimumHeight(px(56));
        connect(btn, SIGNAL(clicked()), SLOT(ask()));   // target 속성이 없으면 0 -> 문구 그대로 질문
        ui->layoutFollow->addWidget(btn);
    }
}

void Admin::forecast()
{
    int days = ui->comboDays->currentIndex() == 0 ? 7 : 14;
    QVariantMap args{{"productId", ui->comboProduct->currentData()}, {"days", days}};
    QVariant data;
    QString err;
    if (!Net::call("AI_FORECAST", args, &data, &err)) {
        Msg::warn(this, "AI 판매 예측", err);
        return;
    }
    QVariantMap res = data.toMap();
    ui->forecastChart->setData(res.value("past").toList(), res.value("future").toList(),
                               ui->comboProduct->currentText() + QString(" - 앞으로 %1일 판매 예측").arg(days));

    QVariantList rows = res.value("rows").toList();
    ui->tableForecast->setRowCount(rows.size());
    int danger = 0;
    for (int i = 0; i < rows.size(); ++i) {
        QVariantMap m = rows.at(i).toMap();
        int left = m.value("daysLeft").toInt();
        int order = m.value("order").toInt();
        ui->tableForecast->setItem(i, 0, cell(m.value("name").toString()));
        ui->tableForecast->setItem(i, 1, cell(QString("%L1개").arg(m.value("stock").toInt()), true));
        ui->tableForecast->setItem(i, 2, cell(QString::number(m.value("avg").toDouble(), 'f', 1) + "개", true));
        ui->tableForecast->setItem(i, 3, cell(QString("%L1개").arg(qRound(m.value("predicted").toDouble())), true));
        ui->tableForecast->setItem(i, 4, cell(left < 0 ? "-" : left == 0 ? "품절" : QString("%1일 후").arg(left)));
        ui->tableForecast->setItem(i, 5, cell(order ? QString("%L1개").arg(order) : "-", true));
        if (left >= 0 && left <= 3) {
            ++danger;
            ui->tableForecast->item(i, 4)->setForeground(QColor("#d03b3b"));
        }
    }
    ui->lblForecastSum->setText(QString("품절 위험(3일 이내) %1개 상품").arg(danger));
}
