#include "cartdialog.h"
#include "ui_cartdialog.h"
#include "facedialog.h"
#include "serverclient.h"

#include "kioskdialog.h"
#include "qtcompat.h"

CartDialog::CartDialog(const QVariantMap &user, QList<CartItem> *cart, QWidget *parent)
    : QDialog(parent), ui(new Ui::CartDialog), m_user(user), m_cart(cart)
{
    ui->setupUi(this);   // 화면 배치: cartdialog.ui ([-][+] 버튼은 refresh()에서 만든다)
    scaleUi(this);
    resize(px(680), px(900));

    stretchColumns(ui->table->horizontalHeader());
    ui->table->verticalHeader()->setDefaultSectionSize(px(48));   // [-][+] 버튼이 들어가는 높이

    // 합계 + 옆에 작은 글씨로 충전 잔액
    ui->balanceLabel->setText(QString("(충전 잔액 %1)").arg(won(m_user.value("balance").toInt())));

    connect(ui->clearButton, SIGNAL(clicked()), SLOT(clearCart()));
    connect(ui->moreButton, SIGNAL(clicked()), SLOT(reject()));
    connect(ui->cardButton, SIGNAL(clicked()), SLOT(payByCard()));
    connect(ui->faceButton, SIGNAL(clicked()), SLOT(payByFace()));

    refresh();
}

CartDialog::~CartDialog()
{
    delete ui;
}

int CartDialog::total() const
{
    int sum = 0;
    foreach (const CartItem &item, *m_cart)
        sum += item.price * item.qty;
    return sum;
}

static QTableWidgetItem *cell(const QString &text, bool number = false)
{
    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setTextAlignment((number ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    return item;
}

void CartDialog::refresh()
{
    ui->table->setRowCount(m_cart->size());
    for (int i = 0; i < m_cart->size(); ++i) {
        const CartItem &item = m_cart->at(i);
        ui->table->setItem(i, 0, cell(item.name));
        ui->table->setItem(i, 1, cell(won(item.price), true));
        ui->table->setItem(i, 2, cell(QString("%1개").arg(item.qty), true));
        ui->table->setItem(i, 3, cell(won(item.price * item.qty), true));

        // [-] [+] 버튼
        QWidget *box = new QWidget;
        QHBoxLayout *h = new QHBoxLayout(box);
        h->setContentsMargins(px(4), px(2), px(4), px(2));
        h->setSpacing(px(6));
        const char *labels[2] = { "−", "+" };
        for (int k = 0; k < 2; ++k) {
            QPushButton *b = new QPushButton(labels[k]);
            b->setFixedSize(px(44), px(38));
            b->setStyleSheet(css("min-height: 0; padding: 0; font-size: 20px; font-weight: bold;"));
            b->setProperty("row", i);
            b->setProperty("delta", k == 0 ? -1 : 1);
            b->setEnabled(k == 0 || item.qty < item.stock);   // 재고만큼만 늘릴 수 있음
            connect(b, SIGNAL(clicked()), SLOT(changeQty()));
            h->addWidget(b);
        }
        ui->table->setCellWidget(i, 4, box);
    }
    ui->totalLabel->setText("합계: " + won(total()));
}

void CartDialog::changeQty()
{
    const int row = sender()->property("row").toInt();
    if (row < 0 || row >= m_cart->size())
        return;
    CartItem &item = (*m_cart)[row];
    item.qty += sender()->property("delta").toInt();
    if (item.qty > item.stock)
        item.qty = item.stock;
    if (item.qty <= 0)
        m_cart->removeAt(row);   // 0개가 되면 장바구니에서 뺀다

    // 지금 눌린 버튼을 표에서 교체하므로, 클릭 처리가 끝난 뒤에 다시 그린다.
    QMetaObject::invokeMethod(this, "refresh", Qt::QueuedConnection);
}

void CartDialog::clearCart()
{
    if (m_cart->isEmpty())
        return;
    if (KioskMessage::question(this, "장바구니 비우기", "장바구니를 모두 비우시겠습니까?",
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;
    m_cart->clear();
    refresh();
}

bool CartDialog::pay(int userId, const QString &method, QVariantMap *result)
{
    if (m_cart->isEmpty()) {
        KioskMessage::information(this, "결제", "장바구니가 비어 있습니다.");
        return false;
    }

    QVariantList items;
    foreach (const CartItem &c, *m_cart) {
        QVariantMap m;
        m["productId"] = c.productId;
        m["qty"] = c.qty;
        items << m;
    }

    QVariant data;
    QString err;
    const QVariantMap args{{"userId", userId}, {"method", method}, {"items", items}};
    if (!ServerClient::call("PAY", args, &data, &err)) {
        KioskMessage::warning(this, "결제 실패", err);
        return false;
    }
    *result = data.toMap();
    return true;
}

void CartDialog::payByCard()
{
    if (m_cart->isEmpty())
        return;
    if (KioskMessage::question(this, "카드 결제",
                              QString("결제 금액: %1\n\n카드를 단말기에 투입(태그)한 후 [예]를 눌러 주세요.").arg(won(total())),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QVariantMap r;
    if (!pay(m_user.value("id").toInt(), "card", &r))
        return;
    KioskMessage::information(this, "결제 완료",
                             QString("%1 카드 결제가 완료되었습니다.\n처음 화면으로 돌아갑니다.").arg(won(r.value("total").toInt())));
    accept();
}

void CartDialog::payByFace()
{
    if (m_cart->isEmpty())
        return;

    FaceDialog face("얼굴인식 결제", this);
    if (execInWindow(&face, true) != QDialog::Accepted)
        return;

    // 인식된 사용자의 충전 잔액 조회
    QVariant data;
    QString err;
    if (!ServerClient::call("LOGIN", QVariantMap{{"userId", face.userId()}}, &data, &err)) {
        KioskMessage::warning(this, "얼굴인식 결제", err);
        return;
    }
    const QVariantMap payer = data.toMap();
    const int balance = payer.value("balance").toInt();
    const int amount = total();

    const QString info = QString("고객: %1\n\n충전 잔액: %2\n차감 금액: -%3\n결제 후 잔액: %4")
                             .arg(payer.value("name").toString())
                             .arg(won(balance)).arg(won(amount)).arg(won(balance - amount));
    if (balance < amount) {
        KioskMessage::warning(this, "잔액 부족", info + "\n\n충전 금액이 부족합니다. 다른 결제수단을 이용해 주세요.");
        return;
    }
    if (KioskMessage::question(this, "얼굴인식 결제", info + "\n\n결제하시겠습니까?",
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QVariantMap r;
    if (!pay(face.userId(), "face", &r))
        return;
    KioskMessage::information(this, "결제 완료",
                             QString("%1이(가) 차감되었습니다.\n남은 잔액: %2\n\n처음 화면으로 돌아갑니다.")
                                 .arg(won(r.value("total").toInt()))
                                 .arg(won(r.value("balance").toInt())));
    accept();
}
