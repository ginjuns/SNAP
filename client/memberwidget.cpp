#include "memberwidget.h"
#include "serverclient.h"

#include "qtcompat.h"

static const int kColumns = 3;
static const int kImageSize = 200;

MemberWidget::MemberWidget(QWidget *parent)
    : QWidget(parent)
{
    m_welcome = new QLabel;
    m_welcome->setStyleSheet("font-size: 22px; font-weight: bold;");
    QPushButton *logout = new QPushButton("처음으로");
    connect(logout, SIGNAL(clicked()), SIGNAL(finished()));

    QHBoxLayout *header = new QHBoxLayout;
    header->addWidget(m_welcome);
    header->addStretch();
    header->addWidget(logout);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addLayout(header);
    layout->addWidget(m_scroll);
}

void MemberWidget::start(const QVariantMap &user)
{
    m_user = user;
    m_cart.clear();
    m_welcome->setText(QString("%1님 환영합니다").arg(user.value("name").toString()));
    loadProducts();
}

void MemberWidget::loadProducts()
{
    QVariant data;
    QString err;
    if (!ServerClient::call("PRODUCTS", QVariantList(), &data, &err))
        QMessageBox::warning(this, "상품 조회 실패", err);
    m_products = data.toList();

    QWidget *grid = new QWidget;
    QGridLayout *gridLayout = new QGridLayout(grid);
    for (int i = 0; i < m_products.size(); ++i) {
        const QVariantMap p = m_products.at(i).toMap();

        QLabel *image = new QLabel;
        image->setFixedSize(kImageSize, kImageSize);
        image->setAlignment(Qt::AlignCenter);
        QPixmap pixmap;
        if (pixmap.loadFromData(p.value("image").toByteArray()))
            image->setPixmap(pixmap.scaled(kImageSize, kImageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            image->setText("이미지 없음");

        QLabel *name = new QLabel(p.value("name").toString());
        QLabel *price = new QLabel(won(p.value("price").toInt()));
        price->setStyleSheet("font-weight: bold; color: #d32f2f;");
        QPushButton *buy = new QPushButton("구매");
        buy->setProperty("index", i);
        connect(buy, SIGNAL(clicked()), SLOT(onBuy()));

        QFrame *card = new QFrame;
        card->setFrameShape(QFrame::StyledPanel);
        QVBoxLayout *v = new QVBoxLayout(card);
        v->addWidget(image, 0, Qt::AlignCenter);
        v->addWidget(name, 0, Qt::AlignCenter);
        v->addWidget(price, 0, Qt::AlignCenter);
        v->addWidget(buy);

        gridLayout->addWidget(card, i / kColumns, i % kColumns);
    }
    gridLayout->setRowStretch(gridLayout->rowCount(), 1);
    m_scroll->setWidget(grid);   // 이전 그리드는 QScrollArea가 삭제
}

void MemberWidget::onBuy()
{
    const QVariantMap p = m_products.value(sender()->property("index").toInt()).toMap();
    const int id = p.value("id").toInt();

    bool found = false;
    for (int i = 0; i < m_cart.size(); ++i) {
        if (m_cart[i].productId == id) {
            ++m_cart[i].qty;
            found = true;
        }
    }
    if (!found) {
        CartItem item = { id, p.value("name").toString(), p.value("price").toInt(), 1 };
        m_cart << item;
    }

    CartDialog dialog(m_user, &m_cart, this);
    if (dialog.exec() == QDialog::Accepted)   // 결제 완료
        emit finished();
}
