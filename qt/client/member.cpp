#include "member.h"
#include "ui_member.h"
#include "net.h"
#include "server.h"
#include "popup.h"
#include "util.h"

static const int COLS = 3;
static const int IMG_SIZE = 180;

Member::Member(QWidget *parent)
    : QWidget(parent), ui(new Ui::Member), m_down(0)
{
    ui->setupUi(this);
    scaleUi(this);
    touchScroll(ui->scroll);
    connect(ui->btnHome, SIGNAL(clicked()), SIGNAL(done()));
    connect(ui->btnCart, SIGNAL(clicked()), SLOT(openCart()));
    if (Esp::get())
        connect(Esp::get(), SIGNAL(shelfChanged(QVariantList)), SLOT(build()));
}

Member::~Member()
{
    delete ui;
}

void Member::start(const QVariantMap &user)
{
    m_user = user;
    m_cart.clear();
    ui->lblWelcome->setText(QString("%1님 환영합니다").arg(user.value("name").toString()));
    ui->lblMsg->clear();
    load();
    updateBar();
}

void Member::load()
{
    QVariant data;
    QString err;
    if (!Net::call("PRODUCTS", QVariantMap(), &data, &err))
        Msg::warn(this, "상품 조회 실패", err);
    m_list = data.toList();
    build();
}

// 상품 카드 다시 그리기 (재고 0 또는 지정한 진열대가 비면 품절)
void Member::build()
{
    m_down = 0;

    QWidget *grid = new QWidget;
    QGridLayout *g = new QGridLayout(grid);
    for (int i = 0; i < m_list.size(); ++i) {
        QVariantMap p = m_list.at(i).toMap();
        bool soldOut = p.value("stock").toInt() <= 0 || Esp::shelfEmpty(p.value("shelf").toInt());

        QLabel *img = new QLabel;
        img->setFixedSize(px(IMG_SIZE), px(IMG_SIZE));
        img->setAlignment(Qt::AlignCenter);
        QPixmap pix;
        if (pix.loadFromData(QByteArray::fromBase64(p.value("image").toString().toLatin1())))
            img->setPixmap(pix.scaled(px(IMG_SIZE), px(IMG_SIZE), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            img->setText("이미지 없음");

        QLabel *name = new QLabel(p.value("name").toString());
        name->setStyleSheet(css("font-size: 17px;"));
        QLabel *price = new QLabel(soldOut ? "품절" : won(p.value("price").toInt()));
        price->setStyleSheet(css(soldOut ? "font-size: 18px; font-weight: bold; color: #d32f2f;"
                                         : "font-size: 18px; font-weight: bold;"));

        QFrame *card = new QFrame;
        card->setObjectName(soldOut ? "soldOut" : "product");
        card->setStyleSheet(css("QFrame#product { border: 1px solid #c3c2b7; border-radius: 8px; background: white; }"
                                "QFrame#product[down=\"true\"] { border: 2px solid #2a78d6; background: #e8f0fb; }"
                                "QFrame#soldOut { border: 2px solid #d32f2f; border-radius: 8px; background: #fdecea; }"));
        QVBoxLayout *v = new QVBoxLayout(card);
        v->addWidget(img, 0, Qt::AlignCenter);
        v->addWidget(name, 0, Qt::AlignCenter);
        v->addWidget(price, 0, Qt::AlignCenter);
        if (!soldOut) {
            card->setProperty("index", i);
            card->setCursor(Qt::PointingHandCursor);
            card->installEventFilter(this);
        }

        g->addWidget(card, i / COLS, i % COLS);
    }
    g->setRowStretch(g->rowCount(), 1);
    ui->scroll->setWidget(grid);
}

static void setDown(QWidget *card, bool down)
{
    card->setProperty("down", down);
    card->style()->unpolish(card);
    card->style()->polish(card);
}

bool Member::eventFilter(QObject *obj, QEvent *e)
{
    QWidget *card = qobject_cast<QWidget *>(obj);
    if (!card)
        return QWidget::eventFilter(obj, e);

    if (e->type() == QEvent::MouseButtonPress) {
        m_down = card;
        m_downPos = static_cast<QMouseEvent *>(e)->globalPos();
        setDown(card, true);
        return true;
    }
    if (e->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent *>(e);
        setDown(card, false);
        bool tap = m_down == card && card->rect().contains(me->pos())
                   && (me->globalPos() - m_downPos).manhattanLength() < px(20);
        m_down = 0;
        if (tap)
            add(card->property("index").toInt());
        return true;
    }
    return QWidget::eventFilter(obj, e);
}

void Member::add(int i)
{
    QVariantMap p = m_list.value(i).toMap();
    int id = p.value("id").toInt();
    QString name = p.value("name").toString();
    int stock = p.value("stock").toInt();

    for (int k = 0; k < m_cart.size(); ++k) {
        if (m_cart[k].id == id) {
            if (m_cart[k].qty >= stock) {
                ui->lblMsg->setText(QString("'%1'은(는) %2개까지만 담을 수 있습니다.").arg(name).arg(stock));
                return;
            }
            ++m_cart[k].qty;
            ui->lblMsg->setText(QString("'%1'을(를) 담았습니다. (%2개)").arg(name).arg(m_cart[k].qty));
            updateBar();
            return;
        }
    }
    Item it = { id, name, p.value("price").toInt(), 1, stock, p.value("shelf").toInt() };
    m_cart << it;
    ui->lblMsg->setText(QString("'%1'을(를) 담았습니다.").arg(name));
    updateBar();
}

void Member::updateBar()
{
    int qty = 0, sum = 0;
    foreach (const Item &it, m_cart) {
        qty += it.qty;
        sum += it.price * it.qty;
    }
    ui->lblQty->setText(QString("총 수량 %L1개").arg(qty));
    ui->lblSum->setText("총 금액 " + won(sum));
}

void Member::openCart()
{
    Cart dlg(m_user, &m_cart, this);
    if (popup(&dlg) == QDialog::Accepted) {
        emit done();
        return;
    }
    ui->lblMsg->clear();
    updateBar();
}
