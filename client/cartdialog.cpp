#include "cartdialog.h"
#include "facedialog.h"
#include "serverclient.h"

#include "qtcompat.h"

CartDialog::CartDialog(const QVariantMap &user, QList<CartItem> *cart, QWidget *parent)
    : QDialog(parent), m_user(user), m_cart(cart)
{
    setWindowTitle("장바구니");
    resize(640, 480);

    m_table = new QTableWidget(0, 4);
    m_table->setHorizontalHeaderLabels(QStringList() << "상품명" << "단가" << "수량" << "금액");
    stretchColumns(m_table->horizontalHeader());
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    m_total = new QLabel;
    m_total->setAlignment(Qt::AlignRight);
    m_total->setStyleSheet("font-size: 22px; font-weight: bold;");

    QPushButton *remove = new QPushButton("선택 삭제");
    QPushButton *more = new QPushButton("계속 쇼핑");
    QPushButton *card = new QPushButton("카드 결제");
    QPushButton *face = new QPushButton("얼굴인식 결제");
    connect(remove, SIGNAL(clicked()), SLOT(removeSelected()));
    connect(more, SIGNAL(clicked()), SLOT(reject()));
    connect(card, SIGNAL(clicked()), SLOT(payByCard()));
    connect(face, SIGNAL(clicked()), SLOT(payByFace()));

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->addWidget(remove);
    buttons->addWidget(more);
    buttons->addStretch();
    buttons->addWidget(card);
    buttons->addWidget(face);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_table);
    layout->addWidget(m_total);
    layout->addLayout(buttons);

    refresh();
}

int CartDialog::total() const
{
    int sum = 0;
    foreach (const CartItem &item, *m_cart)
        sum += item.price * item.qty;
    return sum;
}

void CartDialog::refresh()
{
    m_table->setRowCount(m_cart->size());
    for (int i = 0; i < m_cart->size(); ++i) {
        const CartItem &item = m_cart->at(i);
        m_table->setItem(i, 0, new QTableWidgetItem(item.name));
        m_table->setItem(i, 1, new QTableWidgetItem(won(item.price)));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(item.qty)));
        m_table->setItem(i, 3, new QTableWidgetItem(won(item.price * item.qty)));
    }
    m_total->setText("합계: " + won(total()));
}

void CartDialog::removeSelected()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    m_cart->removeAt(row);
    refresh();
}

bool CartDialog::pay(int userId, const QString &method, QVariantMap *result)
{
    if (m_cart->isEmpty()) {
        QMessageBox::information(this, "결제", "장바구니가 비어 있습니다.");
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
    if (!ServerClient::call("PAY", QVariantList() << userId << method << QVariant(items), &data, &err)) {
        QMessageBox::warning(this, "결제 실패", err);
        return false;
    }
    *result = data.toMap();
    return true;
}

void CartDialog::payByCard()
{
    if (m_cart->isEmpty())
        return;
    if (QMessageBox::question(this, "카드 결제",
                              QString("결제 금액: %1\n\n카드를 단말기에 투입(태그)한 후 [예]를 눌러 주세요.").arg(won(total())),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QVariantMap r;
    if (!pay(m_user.value("id").toInt(), "card", &r))
        return;
    QMessageBox::information(this, "결제 완료",
                             QString("%1 카드 결제가 완료되었습니다.\n처음 화면으로 돌아갑니다.").arg(won(r.value("total").toInt())));
    accept();
}

void CartDialog::payByFace()
{
    if (m_cart->isEmpty())
        return;

    FaceDialog face("얼굴인식 결제", this);
    if (face.exec() != QDialog::Accepted)
        return;

    // 인식된 사용자의 충전 잔액 조회
    QVariant data;
    QString err;
    if (!ServerClient::call("LOGIN", QVariantList() << face.userId(), &data, &err)) {
        QMessageBox::warning(this, "얼굴인식 결제", err);
        return;
    }
    const QVariantMap payer = data.toMap();
    const int balance = payer.value("balance").toInt();
    const int amount = total();

    const QString info = QString("고객: %1\n\n충전 잔액: %2\n차감 금액: -%3\n결제 후 잔액: %4")
                             .arg(payer.value("name").toString())
                             .arg(won(balance)).arg(won(amount)).arg(won(balance - amount));
    if (balance < amount) {
        QMessageBox::warning(this, "잔액 부족", info + "\n\n충전 금액이 부족합니다. 다른 결제수단을 이용해 주세요.");
        return;
    }
    if (QMessageBox::question(this, "얼굴인식 결제", info + "\n\n결제하시겠습니까?",
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    QVariantMap r;
    if (!pay(face.userId(), "face", &r))
        return;
    QMessageBox::information(this, "결제 완료",
                             QString("%1이(가) 차감되었습니다.\n남은 잔액: %2\n\n처음 화면으로 돌아갑니다.")
                                 .arg(won(r.value("total").toInt()))
                                 .arg(won(r.value("balance").toInt())));
    accept();
}
