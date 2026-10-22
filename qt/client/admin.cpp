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

#include <QFutureWatcher>
#include <QPointer>
#include <QtConcurrent>
// 표에 넣을 칸 생성
static QTableWidgetItem *cell(const QString &text, bool num = false)
{
    QTableWidgetItem *c = new QTableWidgetItem(text);
    if (num)
        c->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return c;
}

static const char *NO_IMAGE = "사진 없음\n\n터치하여 추가";

enum Icon { Plus, Pencil, Reset, Trash };

// 상품 관리 버튼 아이콘을 직접 그려서 만듦
static QIcon makeIcon(Icon type)
{
    QPixmap pix(64, 64);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QColor color = (type == Trash) ? QColor("#d32f2f") : (type == Reset) ? QColor("#52514e") : QColor(Theme::GREEN);
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

// 버튼에 아이콘을 화면 배율에 맞춰 붙임
static void setIcon(QPushButton *btn, Icon type)
{
    btn->setIcon(makeIcon(type));
    btn->setIconSize(QSize(px(32), px(32)));
}

// 콤보박스를 터치하기 쉬운 토글 버튼 묶음(일별 | 월별)으로 바꿔 보여 줌
// 콤보박스는 숨겨서 그대로 두고 값만 버튼과 맞춤 (선택 로직은 콤보박스 그대로 사용)
static QFrame *segment(QComboBox *combo)
{
    QFrame *box = new QFrame;
    box->setObjectName("segment");
    QHBoxLayout *h = new QHBoxLayout(box);
    h->setContentsMargins(px(3), px(3), px(3), px(3));
    h->setSpacing(px(2));
    QButtonGroup *group = new QButtonGroup(box);
    for (int i = 0; i < combo->count(); ++i) {
        QPushButton *b = new QPushButton(combo->itemText(i).section(" (", 0, 0));
        b->setCheckable(true);
        b->setChecked(i == combo->currentIndex());
        group->addButton(b, i);
        h->addWidget(b);
    }
    QObject::connect(group, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked),
                     combo, &QComboBox::setCurrentIndex);
    QObject::connect(combo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                     group, [group](int i) { if (group->button(i)) group->button(i)->setChecked(true); });
    combo->setProperty("segment", QVariant::fromValue<QObject *>(group));

    // 콤보박스가 있던 자리(가로/세로/폼 레이아웃 모두)에 토글 묶음을 넣음
    foreach (QLayout *l, combo->parentWidget()->findChildren<QLayout *>()) {
        if (l->indexOf(combo) >= 0) {
            delete l->replaceWidget(combo, box);
            break;
        }
    }
    combo->hide();
    return box;
}

// 신호를 막고 콤보박스 값을 바꿨을 때 토글 버튼도 맞춤
static void syncSegment(QComboBox *combo)
{
    QButtonGroup *group = qobject_cast<QButtonGroup *>(combo->property("segment").value<QObject *>());
    if (group && group->button(combo->currentIndex()))
        group->button(combo->currentIndex())->setChecked(true);
}

// 관리자 화면 초기화: 입력칸 키보드, 버튼/표/콤보박스 설정과 시그널 연결
Admin::Admin(QWidget *parent)
    : QWidget(parent), ui(new Ui::Admin), m_imgChanged(false)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground);
    scaleUi(this);
    connect(ui->btnLogout, SIGNAL(clicked()), SIGNAL(done()));

    Keyboard::attach(ui->editName, Keyboard::Text, "상품명 입력");
    ui->editPrice->setValidator(new QIntValidator(1, 100000000, ui->editPrice));
    Keyboard::attach(ui->editPrice, Keyboard::Number, "가격 입력 (원)");
    ui->editStock->setValidator(new QIntValidator(0, 1000000, ui->editStock));
    Keyboard::attach(ui->editStock, Keyboard::Number, "재고 수량 입력 (개)");

    // 진열대: 없음 | 1번 | 2번 | 3번 | 4번 토글 (입력 폼 폭에 맞게 버튼을 균등하게 나눔)
    ui->comboShelf->addItem("없음");
    for (int i = 1; i <= SHELF_COUNT; ++i)
        ui->comboShelf->addItem(QString("%1번").arg(i));
    segment(ui->comboShelf)->setStyleSheet(css("QPushButton { padding: 0 4px; min-width: 0px; }"));

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
    segment(ui->comboUnit);
    connect(ui->comboUnit, SIGNAL(currentIndexChanged(int)), SLOT(onUnit()));
    connect(ui->date, SIGNAL(dateChanged(QDate)), SLOT(loadSales()));
    connect(ui->btnPrev, SIGNAL(clicked()), SLOT(prev()));
    connect(ui->btnNext, SIGNAL(clicked()), SLOT(next()));
    connect(ui->btnToday, SIGNAL(clicked()), SLOT(today()));

    ui->tableDetail->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tableDetail->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableDetail->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tableDetail->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    touchScroll(ui->tableDetail);
    ui->lblLogo->setPixmap(QPixmap(":/images/snap_logo.png")
                               .scaled(px(40), px(40), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    QList<QPushButton *> menu;
    menu << ui->btnMenuProduct << ui->btnMenuSales << ui->btnMenuReport << ui->btnMenuChat << ui->btnMenuForecast;
    for (int i = 0; i < menu.size(); ++i)
        connect(menu[i], &QPushButton::clicked, this, [this, i]() { ui->stack->setCurrentIndex(i); });

    ui->splitter->setHandleWidth(px(16));
    ui->splitter->setStretchFactor(0, 1);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setSizes(QList<int>() << px(420) << px(580));   // 그래프(왼쪽) 4 : 표(오른쪽) 6

    connect(ui->btnReport, SIGNAL(clicked()), SLOT(makeReport()));
    ui->dateReport->setDate(QDate::currentDate());
    ui->dateReport->setMaximumDate(QDate::currentDate());
    ui->dateReport->findChild<QLineEdit *>()->installEventFilter(this);
    segment(ui->comboPeriod);
    segment(ui->comboDays);
    connect(ui->comboPeriod, SIGNAL(currentIndexChanged(int)), SLOT(onReportUnit()));
    connect(ui->comboCategory, SIGNAL(currentIndexChanged(int)), SLOT(onCategory()));
    connect(ui->btnForecast, SIGNAL(clicked()), SLOT(forecast()));
    foreach (QComboBox *c, QList<QComboBox *>() << ui->comboCategory << ui->comboTarget
                                                << ui->comboProduct) {
        c->setView(new QListView);
        c->setStyleSheet(css("QComboBox QAbstractItemView::item { min-height: 48px; }"));
    }
    ui->textReport->setTextInteractionFlags(Qt::NoTextInteraction);
    ui->textKpi->setTextInteractionFlags(Qt::NoTextInteraction);
    touchScroll(ui->textReport);
    touchScroll(ui->textKpi);
    touchScroll(ui->scrollChat);
    touchScroll(ui->tableForecast);
    stretch(ui->tableForecast);
}

Admin::~Admin()
{
    delete ui;
}

// 관리자가 로그인할 때마다 이전 관리자가 보던 화면을 모두 처음 상태로 되돌린다.
void Admin::start(const QVariantMap &user)
{
    ui->lblWelcome->setText(user.value("name").toString());
    ui->btnMenuProduct->setChecked(true);
    ui->stack->setCurrentIndex(0);

    reset();
    loadProducts();
    ui->tableProduct->scrollToTop();

    // 매출 확인: 일별 / 오늘
    ui->comboUnit->blockSignals(true);
    ui->comboUnit->setCurrentIndex(0);
    ui->comboUnit->blockSignals(false);
    syncSegment(ui->comboUnit);
    ui->date->setDisplayFormat("yyyy-MM-dd");
    ui->date->blockSignals(true);
    ui->date->setDate(QDate::currentDate());
    ui->date->blockSignals(false);
    loadSales();
    ui->tableDetail->scrollToTop();

    // AI 리포트: 일별 / 오늘
    ui->comboPeriod->setCurrentIndex(0);
    ui->dateReport->setMaximumDate(QDate::currentDate());
    ui->dateReport->setDate(QDate::currentDate());
    onReportUnit();
    ui->textReport->clear();
    ui->textKpi->clear();

    // AI 상담: 대화 내용과 추천 질문 지우기
    ++m_chat;
    for (int i = ui->layoutChat->count() - 1; i >= 0; --i) {
        QLayoutItem *item = ui->layoutChat->itemAt(i);
        if (item->widget()) {
            ui->layoutChat->takeAt(i);
            item->widget()->deleteLater();
            delete item;
        }
    }
    setFollowUps(QStringList());
    ui->comboCategory->blockSignals(true);
    ui->comboCategory->setCurrentIndex(0);
    ui->comboCategory->blockSignals(false);

    // AI 판매 예측
    ui->comboDays->setCurrentIndex(0);
    ui->forecastChart->setData(QVariantList(), QVariantList(), QString());
    ui->tableForecast->setRowCount(0);
    ui->lblForecastSum->clear();

    loadTargets();
    onCategory();
}

// 상품 입력칸과 사진을 비움
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

// 사진 영역 터치 → 사진 선택, 날짜 칸 터치 → 달력 열기
bool Admin::eventFilter(QObject *obj, QEvent *e)
{
    if (obj == ui->lblImage && e->type() == QEvent::MouseButtonRelease) {
        pickImage();
        return true;
    }
    QDateEdit *edit = obj->parent() == ui->date ? ui->date : obj->parent() == ui->dateReport ? ui->dateReport : 0;
    if (edit) {
        if (e->type() == QEvent::MouseButtonRelease)
            pickDate(edit);
        if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease
            || e->type() == QEvent::MouseButtonDblClick)
            return true;
    }
    return QWidget::eventFilter(obj, e);
}

// 달력 창에서 날짜(월별이면 월)를 골라 매출 확인 / AI 리포트 날짜 칸에 넣음
void Admin::pickDate(QDateEdit *edit)
{
    bool month = edit == ui->date ? monthly() : ui->comboPeriod->currentIndex() == 2;
    Calendar dlg(edit->date(), month, this);
    if (popup(&dlg) == QDialog::Accepted)
        edit->setDate(qMin(dlg.date(), edit->maximumDate()));
    if (edit == ui->dateReport)
        onReportUnit();
}

// AI 리포트의 일별/주별/월별이나 날짜를 바꾸면 날짜 표시 형식과 기간 안내 문구 갱신
void Admin::onReportUnit()
{
    int unit = ui->comboPeriod->currentIndex();
    QDate d = ui->dateReport->date();
    ui->dateReport->setDisplayFormat(unit == 2 ? "yyyy년 MM월" : "yyyy-MM-dd");
    QString period = unit == 0 ? QLocale().toString(d, "yyyy년 M월 d일 (ddd)")
                   : unit == 1 ? QString("%1 ~ %2").arg(d.addDays(1 - d.dayOfWeek()).toString("yyyy-MM-dd"),
                                                       d.addDays(7 - d.dayOfWeek()).toString("yyyy-MM-dd"))
                               : d.toString("yyyy년 M월");
    ui->lblReportInfo->setText(QString("기간: %1  ·  [AI 리포트 생성]을 누르세요.").arg(period));
}

// 상품 사진 파일을 골라 300px 이하 PNG 로 줄여 둠
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

// 상품 목록에서 선택한 행 번호 (없으면 -1)
int Admin::row() const
{
    QList<QTableWidgetItem *> items = ui->tableProduct->selectedItems();
    return items.isEmpty() ? -1 : items.first()->row();
}

// 입력칸에서 상품명/가격/재고를 읽음, 빈 칸이 있으면 안내하고 false
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

// 상품 추가
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

// 선택한 상품 수정, 사진은 바꿨을 때만 보냄
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

// 선택한 상품 삭제, 매출 기록은 남음
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

// 목록에서 상품을 고르면 입력칸과 사진에 그 상품 정보를 채움
void Admin::onSelect()
{
    markShelves();
    int r = row();
    if (r < 0)
        return;
    QVariantMap p = m_list.value(r).toMap();
    ui->editName->setText(p.value("name").toString());
    ui->editPrice->setText(QString::number(p.value("price").toInt()));
    ui->editStock->setText(QString::number(p.value("stock").toInt()));
    int shelf = p.value("shelf").toInt();
    ui->comboShelf->setCurrentIndex(shelf <= SHELF_COUNT ? shelf : 0);

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

// 다른 상품이 이미 쓰는 진열대 버튼은 못 누르게 막음 (선택한 상품 자신의 진열대는 허용)
void Admin::markShelves()
{
    QButtonGroup *group = qobject_cast<QButtonGroup *>(ui->comboShelf->property("segment").value<QObject *>());
    if (!group)
        return;
    int r = row();
    for (int i = 1; i <= SHELF_COUNT; ++i) {
        bool used = false;
        for (int j = 0; j < m_list.size(); ++j)
            if (j != r && m_list.at(j).toMap().value("shelf").toInt() == i)
                used = true;
        if (group->button(i))
            group->button(i)->setEnabled(!used);
    }
}

// 서버에서 상품 목록을 받아 상품 관리 표에 표시
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
        QString s = shelf && shelf <= SHELF_COUNT ? QString("%1번").arg(shelf) : QString("-");
        if (Esp::shelfEmpty(shelf))
            s += " (비어 있음)";
        ui->tableProduct->setItem(i, 3, cell(s));
    }

    // 판매 예측의 상품 선택 목록도 같이 갱신 (상품을 추가/수정/삭제해도 바로 반영)
    int selected = ui->comboProduct->currentData().toInt();
    ui->comboProduct->clear();
    ui->comboProduct->addItem("전체 상품", 0);
    foreach (const QVariant &v, m_list) {
        QVariantMap m = v.toMap();
        ui->comboProduct->addItem(m.value("name").toString(), m.value("id"));
    }
    ui->comboProduct->setCurrentIndex(qMax(0, ui->comboProduct->findData(selected)));
    markShelves();
}

// 매출 확인이 월별 보기인지
bool Admin::monthly() const
{
    return ui->comboUnit->currentIndex() == 1;
}

// 일별/월별을 바꾸면 날짜 표시 형식을 바꾸고 매출 다시 조회
void Admin::onUnit()
{
    ui->date->setDisplayFormat(monthly() ? "yyyy년 MM월" : "yyyy-MM-dd");
    loadSales();
}

// 이전 날(월)로 이동
void Admin::prev()
{
    ui->date->setDate(monthly() ? ui->date->date().addMonths(-1) : ui->date->date().addDays(-1));
}

// 다음 날(월)로 이동
void Admin::next()
{
    ui->date->setDate(monthly() ? ui->date->date().addMonths(1) : ui->date->date().addDays(1));
}

// 매출 조회: 그래프, 합계 줄, 시간대(날짜)별 상품 판매 표 갱신
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

    // 일별은 시간대, 월별은 날짜마다 상품별로 묶어서 결제수단별 판매량을 보여 준다.
    struct Sold { int cardQty, cardAmt, faceQty, faceAmt; };
    QMap<QString, QMap<QString, Sold> > groups;
    int qtySum = 0;
    foreach (const QVariant &v, res.value("details").toList()) {
        QVariantMap m = v.toMap();
        QString when = m.value("soldAt").toString();
        QString key = mon ? when.mid(5, 5) : when.mid(11, 2);
        Sold &s = groups[key][m.value("product").toString()];
        int qty = m.value("qty").toInt();
        int amount = m.value("amount").toInt();
        if (m.value("method").toString() == "face") {
            s.faceQty += qty;
            s.faceAmt += amount;
        } else {
            s.cardQty += qty;
            s.cardAmt += amount;
        }
        qtySum += qty;
    }

    QList<QStringList> rows;
    QMapIterator<QString, QMap<QString, Sold> > g(groups);
    g.toBack();
    while (g.hasPrevious()) {
        g.previous();
        QString label = mon ? QString("%1월 %2일").arg(g.key().left(2).toInt()).arg(g.key().mid(3).toInt())
                            : QString("%1시").arg(g.key().toInt());
        QList<QPair<int, QString> > order;
        for (QMap<QString, Sold>::const_iterator it = g.value().constBegin(); it != g.value().constEnd(); ++it)
            order << qMakePair(-(it.value().cardAmt + it.value().faceAmt), it.key());
        std::sort(order.begin(), order.end());
        for (int i = 0; i < order.size(); ++i) {
            const Sold &s = g.value()[order[i].second];
            QString card = s.cardQty ? QString("%L1개 / %2").arg(s.cardQty).arg(won(s.cardAmt)) : QString("-");
            QString face = s.faceQty ? QString("%L1개 / %2").arg(s.faceQty).arg(won(s.faceAmt)) : QString("-");
            QString total = QString("%L1개 / %2").arg(s.cardQty + s.faceQty).arg(won(s.cardAmt + s.faceAmt));
            rows << (QStringList() << (i == 0 ? label : QString()) << order[i].second << card << face << total);
        }
    }

    ui->tableDetail->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        for (int c = 0; c < 5; ++c)
            ui->tableDetail->setItem(i, c, cell(rows[i][c], c >= 2));
        QFont bold = ui->tableDetail->item(i, 4)->font();
        bold.setBold(true);
        ui->tableDetail->item(i, 4)->setFont(bold);
    }
    ui->tableDetail->setHorizontalHeaderLabels(QStringList() << (mon ? "날짜" : "시간") << "상품"
                                                             << "카드" << "얼굴인식" << "합계");
    ui->lblDetail->setText(mon ? "날짜별 상품 판매" : "시간대별 상품 판매");

    // 내용이 표 폭보다 넓으면 모든 칸을 표 폭에 나눠 맞춤 (가로 스크롤 없이 한 화면에 보이게)
    QHeaderView *hd = ui->tableDetail->horizontalHeader();
    hd->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableDetail->resizeColumnsToContents();
    int need = hd->length();
    hd->setSectionResizeMode(QHeaderView::ResizeToContents);
    hd->setSectionResizeMode(1, QHeaderView::Stretch);
    if (need > ui->tableDetail->viewport()->width())
        hd->setSectionResizeMode(QHeaderView::Stretch);

    // 요약 카드: 작은 제목 + 큰 값
    QString kpi = "<span style='font-size:%1px; color:#3d6b4c'>%2</span><br>"
                   "<span style='font-size:%3px; font-weight:bold; color:#14532d'>%4</span>";
    ui->lblKpiTotal->setText(kpi.arg(px(14)).arg("합계").arg(px(24)).arg(won(cardSum + faceSum)));
    ui->lblKpiCard->setText(kpi.arg(px(14)).arg("카드").arg(px(24)).arg(won(cardSum)));
    ui->lblKpiFace->setText(kpi.arg(px(14)).arg("얼굴인식").arg(px(24)).arg(won(faceSum)));
    ui->lblKpiQty->setText(kpi.arg(px(14)).arg("판매수량").arg(px(24)).arg(QString("%L1개").arg(qtySum)));
}

// 오늘 날짜로 이동 (이미 오늘이면 다시 조회)
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

// 레이아웃 안의 위젯을 모두 지움
static void clearLayout(QLayout *layout)
{
    while (QLayoutItem *item = layout->takeAt(0)) {
        if (item->widget())
            item->widget()->deleteLater();
        delete item;
    }
}

// AI 상담에서 고를 회원 목록 준비 (상품 목록은 loadProducts 가 채움)
void Admin::loadTargets()
{
    QVariant data;
    QString err;
    if (Net::call("MEMBERS", QVariantMap(), &data, &err))
        m_members = data.toList();
}

// AI 리포트 생성: 고른 날짜의 일별/주별/월별 리포트를 받아 표시
void Admin::makeReport()
{
    static const char *UNITS[] = {"day", "week", "month"};

    ui->btnReport->setEnabled(false);
    ui->lblReportInfo->setText("AI가 분석 중입니다... (최대 1분)");
    qApp->processEvents();

    QVariant data;
    QString err;
    QVariantMap args{{"unit", UNITS[ui->comboPeriod->currentIndex()]},
                     {"date", ui->dateReport->date().toString("yyyy-MM-dd")}};
    bool ok = Net::call("AI_REPORT", args, &data, &err);
    ui->btnReport->setEnabled(true);
    if (!ok) {
        ui->lblReportInfo->setText("리포트 생성 실패");
        Msg::warn(this, "AI 리포트", err);
        return;
    }
    QVariantMap res = data.toMap();
    ui->textKpi->setHtml(res.value("kpiHtml").toString());
    ui->textReport->setHtml(res.value("aiHtml").toString());
    ui->lblReportInfo->setText(QString("%1  ·  %2 생성")
                                   .arg(res.value("title").toString(),
                                        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")));
}

// AI 상담 질문 분류를 바꾸면 질문 버튼과 대상(상품/회원) 선택 목록을 다시 만듦
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

// 질문 버튼을 누르면 AI 에게 질문하고 대화창에 답과 추천 질문 표시
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
    args["token"] = Net::token();

    // 질문과 '분석 중'을 먼저 보여 주고, AI 답은 백그라운드에서 기다림 (그동안 화면이 멈추지 않음)
    if (m_asking)
        return;
    m_asking = true;
    addBubble(text, true);
    QPointer<QLabel> wait = addBubble("AI가 분석 중...", false);
    flushChat();

    int chat = m_chat;
    QFutureWatcher<QVariantMap> *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, wait, chat]() {
        QVariantMap r = watcher->result();
        watcher->deleteLater();
        m_asking = false;
        if (chat != m_chat)
            return;
        if (wait) {
            ui->layoutChat->removeWidget(wait);
            wait->hide();
            wait->deleteLater();
        }
        if (!r.value("ok").toBool()) {
            addBubble("오류: " + r.value("err").toString(), false);
        } else {
            QVariantMap res = r.value("data").toMap();
            addBubble(res.value("answer").toString(), false);
            setFollowUps(res.value("followUps").toStringList());
        }
        flushChat();
    });
    watcher->setFuture(QtConcurrent::run([args]() {
        QVariant data;
        QString err;
        bool ok = Net::call("AI_ASK", args, &data, &err);
        return QVariantMap{{"ok", ok}, {"data", data}, {"err", err}};
    }));
}

// 대화창 배치를 바로 끝내고 맨 아래로 스크롤
void Admin::flushChat()
{
    for (int i = 0; i < 3; ++i) {
        ui->layoutChat->activate();
        QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);
    }
    QScrollBar *bar = ui->scrollChat->verticalScrollBar();
    bar->setValue(bar->maximum());
    ui->scrollChat->viewport()->repaint();
}

// AI 상담 대화창에 말풍선 추가
QLabel *Admin::addBubble(const QString &text, bool mine)
{
    QLabel *lbl = new QLabel(text);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(css(mine ? "background: #1f7a43; color: white; border-radius: 12px; padding: 10px; font-size: 17px;"
                                : "background: #cdffcd; color: #17301f; border-radius: 12px; padding: 10px; font-size: 17px;"));

    QFont f = lbl->font();
    f.setPixelSize(px(17));
    lbl->setFont(f);
    QFontMetrics fm(f);
    int pad = px(10) * 2 + px(6);
    int maxWidth = qMax(px(300), ui->scrollChat->viewport()->width() * 3 / 4);
    QRect r = fm.boundingRect(QRect(0, 0, maxWidth - pad, 100000), Qt::TextWordWrap, text);
    lbl->setFixedSize(r.width() + pad, r.height() + pad);
    ui->layoutChat->addWidget(lbl, 0, mine ? Qt::AlignRight : Qt::AlignLeft);

    QTimer::singleShot(50, this, [this]() {
        QScrollBar *bar = ui->scrollChat->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
    return lbl;
}

// AI 가 추천한 이어서 물어볼 질문을 버튼으로 표시
void Admin::setFollowUps(const QStringList &list)
{
    clearLayout(ui->layoutFollow);
    foreach (const QString &s, list) {
        QPushButton *btn = new QPushButton(s);
        btn->setMinimumHeight(px(56));
        connect(btn, SIGNAL(clicked()), SLOT(ask()));
        ui->layoutFollow->addWidget(btn);
    }
}

// 판매 예측: 예측 그래프와 품절 예상일·발주 추천 표 갱신
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
        ui->tableForecast->setItem(i, 4, cell(left < 0 ? "-" : left == 0 ? "품절" : QString("%1일 후").arg(left), true));
        ui->tableForecast->setItem(i, 5, cell(order ? QString("%L1개").arg(order) : "-", true));
        if (left >= 0 && left <= 3) {
            ++danger;
            ui->tableForecast->item(i, 4)->setForeground(QColor("#d03b3b"));
        }
    }
    ui->lblForecastSum->setText(QString("품절 위험(3일 이내) %1개 상품").arg(danger));
}
