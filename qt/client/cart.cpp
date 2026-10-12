#include "cart.h"
#include "ui_cart.h"
#include "server.h"
#include "face.h"
#include "net.h"
#include "popup.h"
#include "util.h"

// 장바구니 창 초기화: 표/버튼 설정, ESP32 진열대 변경·카드 태그 신호 연결
Cart::Cart(const QVariantMap &user, QList<Item> *items, QWidget *parent)
    : QDialog(parent), ui(new Ui::Cart), m_user(user), m_items(items)
{
    ui->setupUi(this);
    scaleUi(this);
    resize(px(680), px(900));

    stretch(ui->table);
    touchScroll(ui->table);
    ui->table->verticalHeader()->setDefaultSectionSize(px(48));
    ui->lblBalance->setText(QString("(충전 잔액 %1)").arg(won(m_user.value("balance").toInt())));

    connect(ui->btnClear, SIGNAL(clicked()), SLOT(clearAll()));
    connect(ui->btnBack, SIGNAL(clicked()), SLOT(reject()));
    connect(ui->btnCard, SIGNAL(clicked()), SLOT(payCard()));
    connect(ui->btnFace, SIGNAL(clicked()), SLOT(payFace()));
    ui->btnFace->setVisible(!m_user.value("pwLogin").toBool());
    if (Esp::get()) {
        connect(Esp::get(), SIGNAL(shelfChanged(QVariantList)), SLOT(refresh()));
        connect(Esp::get(), SIGNAL(card(QString)), SLOT(onCard(QString)));
    }

    refresh();
}

Cart::~Cart()
{
    delete ui;
}

// 장바구니 총 금액
int Cart::total() const
{
    int sum = 0;
    foreach (const Item &it, *m_items)
        sum += it.price * it.qty;
    return sum;
}

// 표에 넣을 칸 생성
static QTableWidgetItem *cell(const QString &text, bool num = false)
{
    QTableWidgetItem *c = new QTableWidgetItem(text);
    c->setTextAlignment((num ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    return c;
}

// 장바구니 표 다시 그리기 (품절 표시, [-][+] 버튼, 합계)
void Cart::refresh()
{
    ui->table->setRowCount(m_items->size());
    for (int i = 0; i < m_items->size(); ++i) {
        const Item &it = m_items->at(i);
        bool soldOut = Esp::shelfEmpty(it.shelf);
        QTableWidgetItem *name = cell(soldOut ? it.name + " (품절)" : it.name);
        if (soldOut)
            name->setForeground(QColor("#d32f2f"));
        ui->table->setItem(i, 0, name);
        ui->table->setItem(i, 1, cell(won(it.price), true));
        ui->table->setItem(i, 2, cell(QString("%1개").arg(it.qty), true));
        ui->table->setItem(i, 3, cell(won(it.price * it.qty), true));

        QWidget *box = new QWidget;
        QHBoxLayout *h = new QHBoxLayout(box);
        h->setContentsMargins(px(4), px(2), px(4), px(2));
        h->setSpacing(px(6));
        const char *text[2] = { "−", "+" };
        for (int k = 0; k < 2; ++k) {
            QPushButton *b = new QPushButton(text[k]);
            b->setFixedSize(px(44), px(38));
            b->setStyleSheet(css("min-height: 0; padding: 0; font-size: 20px; font-weight: bold;"));
            b->setProperty("row", i);
            b->setProperty("delta", k == 0 ? -1 : 1);
            b->setEnabled(k == 0 || (!soldOut && it.qty < it.stock));
            connect(b, SIGNAL(clicked()), SLOT(change()));
            h->addWidget(b);
        }
        ui->table->setCellWidget(i, 4, box);
    }
    ui->lblTotal->setText("합계: " + won(total()));
}

// [-]/[+] 버튼: 수량 변경, 0개가 되면 장바구니에서 뺌
void Cart::change()
{
    int row = sender()->property("row").toInt();
    if (row < 0 || row >= m_items->size())
        return;
    Item &it = (*m_items)[row];
    int delta = sender()->property("delta").toInt();
    if (delta > 0 && Esp::shelfEmpty(it.shelf))
        return;
    it.qty += delta;
    if (it.qty > it.stock)
        it.qty = it.stock;
    if (it.qty <= 0)
        m_items->removeAt(row);

    QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
}

// 장바구니 비우기
void Cart::clearAll()
{
    if (m_items->isEmpty())
        return;
    if (!Msg::ask(this, "장바구니 비우기", "장바구니를 모두 비우시겠습니까?"))
        return;
    m_items->clear();
    refresh();
}

// 장바구니에 품절 상품이 있으면 안내하고 false
bool Cart::checkSoldOut()
{
    QStringList names;
    foreach (const Item &it, *m_items)
        if (Esp::shelfEmpty(it.shelf))
            names << it.name;
    if (names.isEmpty())
        return true;
    Msg::warn(this, "품절 상품",
              QString("%1 상품이 품절되었습니다.\n장바구니에서 빼고 결제해 주세요.").arg(names.join(", ")));
    return false;
}

// 서버에 결제 요청, 성공하면 ESP32 부저 / 카드 결제 실패면 CARD_FAIL
bool Cart::pay(int userId, const QString &method, QVariantMap *out, const QString &cardUid)
{
    if (m_items->isEmpty()) {
        Msg::info(this, "결제", "장바구니가 비어 있습니다.");
        return false;
    }
    if (!checkSoldOut())
        return false;

    QVariantList list;
    foreach (const Item &it, *m_items) {
        QVariantMap m;
        m["productId"] = it.id;
        m["qty"] = it.qty;
        list << m;
    }

    QVariant data;
    QString err;
    QVariantMap args{{"userId", userId}, {"method", method}, {"cardUid", cardUid}, {"items", list}};
    if (!Net::call("PAY", args, &data, &err)) {
        if (method == "card" && Esp::get())
            Esp::get()->cardFail();
        Msg::warn(this, "결제 실패", err);
        return false;
    }
    *out = data.toMap();
    if (Esp::get())
        Esp::get()->buzzer();
    return true;
}

// 카드 결제: 리더기를 켜고 카드가 태그될 때까지 기다려 결제, 실패하면 다른 카드 대기
void Cart::payCard()
{
    if (m_items->isEmpty() || !checkSoldOut())
        return;
    Esp *esp = Esp::get();
    if (!esp || !esp->connected()) {
        Msg::warn(this, "카드 결제", "카드 리더기(ESP32)가 연결되어 있지 않습니다.\n얼굴인식 결제를 이용해 주세요.");
        return;
    }
    esp->cardSelect();
    QVariantMap r;
    for (;;) {
        if (!Msg::wait(this, "카드 결제",
                       QString("결제 금액: %1\n\n카드를 리더기에 대 주세요.").arg(won(total())),
                       esp, SIGNAL(card(QString))) || !checkSoldOut()) {
            esp->cardCancel();
            return;
        }
        if (pay(m_user.value("id").toInt(), "card", &r, m_cardUid))
            break;
    }
    Msg::info(this, "결제 완료",
              QString("%1 카드 결제가 완료되었습니다.\n카드 잔액: %2\n\n처음 화면으로 돌아갑니다.")
                  .arg(won(r.value("total").toInt()))
                  .arg(won(r.value("balance").toInt())));
    accept();
}

// 얼굴인식 결제: 얼굴을 다시 확인하고 잔액을 보여 준 뒤 충전 잔액에서 결제
void Cart::payFace()
{
    if (m_user.value("pwLogin").toBool())
        return;
    if (m_items->isEmpty() || !checkSoldOut())
        return;

    Face face("얼굴인식 결제", this);
    if (popup(&face, true) != QDialog::Accepted)
        return;

    QVariant data;
    QString err;
    if (!Net::call("LOGIN", QVariantMap{{"userId", face.id()}}, &data, &err)) {
        Msg::warn(this, "얼굴인식 결제", err);
        return;
    }
    QVariantMap user = data.toMap();
    int balance = user.value("balance").toInt();
    int amount = total();

    QString text = QString("고객: %1\n\n충전 잔액: %2\n차감 금액: -%3\n결제 후 잔액: %4")
                       .arg(user.value("name").toString())
                       .arg(won(balance)).arg(won(amount)).arg(won(balance - amount));
    if (balance < amount) {
        Msg::warn(this, "잔액 부족", text + "\n\n충전 금액이 부족합니다. 다른 결제수단을 이용해 주세요.");
        return;
    }
    if (!Msg::ask(this, "얼굴인식 결제", text + "\n\n결제하시겠습니까?"))
        return;

    QVariantMap r;
    if (!pay(face.id(), "face", &r))
        return;
    Msg::info(this, "결제 완료",
              QString("%1이(가) 차감되었습니다.\n남은 잔액: %2\n\n처음 화면으로 돌아갑니다.")
                  .arg(won(r.value("total").toInt()))
                  .arg(won(r.value("balance").toInt())));
    accept();
}
