#include "memberwidget.h"
#include "ui_memberwidget.h"
#include "serverclient.h"

#include "kioskdialog.h"
#include "qtcompat.h"

static const int kColumns = 3;
static const int kImageSize = 180;   // 3열이 세로 화면 너비(720)에 들어가는 크기

MemberWidget::MemberWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::MemberWidget)
{
    ui->setupUi(this);   // 화면 배치: memberwidget.ui (상품 카드는 loadProducts()에서 만든다)
    scaleUi(this);
    connect(ui->logoutButton, SIGNAL(clicked()), SIGNAL(finished()));
    connect(ui->cartButton, SIGNAL(clicked()), SLOT(openCart()));
}

MemberWidget::~MemberWidget()
{
    delete ui;
}

void MemberWidget::start(const QVariantMap &user)
{
    m_user = user;
    m_cart.clear();
    ui->welcomeLabel->setText(QString("%1님 환영합니다").arg(user.value("name").toString()));
    ui->noticeLabel->clear();
    loadProducts();
    updateCartBar();
}

void MemberWidget::loadProducts()
{
    QVariant data;
    QString err;
    if (!ServerClient::call("PRODUCTS", QVariantMap(), &data, &err))
        KioskMessage::warning(this, "상품 조회 실패", err);
    m_products = data.toList();

    QWidget *grid = new QWidget;
    QGridLayout *gridLayout = new QGridLayout(grid);
    for (int i = 0; i < m_products.size(); ++i) {
        const QVariantMap p = m_products.at(i).toMap();
        const bool soldOut = p.value("stock").toInt() <= 0;

        QLabel *image = new QLabel;
        image->setFixedSize(px(kImageSize), px(kImageSize));
        image->setAlignment(Qt::AlignCenter);
        QPixmap pixmap;
        if (pixmap.loadFromData(QByteArray::fromBase64(p.value("image").toString().toLatin1())))
            image->setPixmap(pixmap.scaled(px(kImageSize), px(kImageSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            image->setText("이미지 없음");

        QLabel *name = new QLabel(p.value("name").toString());
        name->setStyleSheet(css("font-size: 17px;"));
        QLabel *price = new QLabel(soldOut ? "품절" : won(p.value("price").toInt()));
        price->setStyleSheet(css(soldOut ? "font-size: 18px; font-weight: bold; color: #d32f2f;"
                                         : "font-size: 18px; font-weight: bold;"));

        // 카드 전체가 버튼 역할. 품절이면 빨간 테두리/배경으로 표시하고 터치해도 담기지 않는다.
        QFrame *card = new QFrame;
        card->setObjectName(soldOut ? "soldOut" : "product");
        card->setStyleSheet(css("QFrame#product { border: 1px solid #c3c2b7; border-radius: 8px; background: white; }"
                                "QFrame#soldOut { border: 2px solid #d32f2f; border-radius: 8px; background: #fdecea; }"));
        QVBoxLayout *v = new QVBoxLayout(card);
        v->addWidget(image, 0, Qt::AlignCenter);
        v->addWidget(name, 0, Qt::AlignCenter);
        v->addWidget(price, 0, Qt::AlignCenter);
        if (!soldOut) {
            card->setProperty("index", i);
            card->setCursor(Qt::PointingHandCursor);
            card->installEventFilter(this);
        }

        gridLayout->addWidget(card, i / kColumns, i % kColumns);
    }
    gridLayout->setRowStretch(gridLayout->rowCount(), 1);
    ui->scrollArea->setWidget(grid);   // 이전 그리드는 QScrollArea가 삭제
}

bool MemberWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
        return true;   // 누름을 카드가 받아야 뗄 때 이벤트도 카드로 온다
    if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *card = qobject_cast<QWidget *>(watched);
        // 누른 채로 카드 밖에서 뗀 경우는 무시
        if (card && card->rect().contains(static_cast<QMouseEvent *>(event)->pos()))
            addToCart(card->property("index").toInt());
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void MemberWidget::addToCart(int index)
{
    const QVariantMap p = m_products.value(index).toMap();
    const int id = p.value("id").toInt();
    const QString name = p.value("name").toString();
    const int stock = p.value("stock").toInt();

    for (int i = 0; i < m_cart.size(); ++i) {
        if (m_cart[i].productId == id) {
            if (m_cart[i].qty >= stock) {   // 최종 확인은 결제 시 서버에서 다시 함
                ui->noticeLabel->setText(QString("'%1'은(는) %2개까지만 담을 수 있습니다.").arg(name).arg(stock));
                return;
            }
            ++m_cart[i].qty;
            ui->noticeLabel->setText(QString("'%1'을(를) 담았습니다. (%2개)").arg(name).arg(m_cart[i].qty));
            updateCartBar();
            return;
        }
    }
    CartItem item = { id, name, p.value("price").toInt(), 1, stock };
    m_cart << item;
    ui->noticeLabel->setText(QString("'%1'을(를) 담았습니다.").arg(name));
    updateCartBar();
}

void MemberWidget::updateCartBar()
{
    int qty = 0, sum = 0;
    foreach (const CartItem &item, m_cart) {
        qty += item.qty;
        sum += item.price * item.qty;
    }
    ui->totalQtyLabel->setText(QString("총 수량 %L1개").arg(qty));
    ui->totalPriceLabel->setText("총 금액 " + won(sum));
}

void MemberWidget::openCart()
{
    CartDialog dialog(m_user, &m_cart, this);
    if (execInWindow(&dialog) == QDialog::Accepted) {   // 결제 완료
        emit finished();
        return;
    }
    ui->noticeLabel->clear();
    updateCartBar();   // 장바구니에서 수량을 바꿨을 수 있음
}
