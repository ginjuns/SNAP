#include "popup.h"
#include "util.h"

static bool blocked = false;
static QList<QDialog *> opened;

// 절전 중에는 새 팝업을 띄우지 않도록 막음
void setBlocked(bool on)
{
    blocked = on;
}

// 열려 있는 팝업을 모두 닫음 (절전 모드 들어갈 때)
void closeAll()
{
    QList<QDialog *> list = opened;
    for (int i = list.size() - 1; i >= 0; --i)
        list.at(i)->reject();
}

// 팝업을 열린 목록에 넣고 실행 (닫을 때 목록에서 뺌)
static int run(QDialog *dlg)
{
    opened << dlg;
    int ret = dlg->exec();
    opened.removeAll(dlg);
    return ret;
}

// 팝업을 반투명 배경 위 화면 가운데(full 이면 전체 화면)에 띄우고 결과를 돌려줌
int popup(QDialog *dlg, bool full)
{
    if (blocked)
        return QDialog::Rejected;

    QWidget *old = dlg->parentWidget();
    QWidget *top = old ? old->window() : qApp->activeWindow();
    if (!top)
        return run(dlg);

    bool moved = dlg->testAttribute(Qt::WA_Moved);
    QPoint pos = dlg->pos();

    QWidget bg(top);
    bg.setObjectName("bg");
    bg.setAttribute(Qt::WA_StyledBackground);
    bg.setStyleSheet("QWidget#bg { background: rgba(0, 0, 0, 110); }");
    bg.setGeometry(top->rect());
    bg.show();
    bg.raise();

    dlg->setParent(&bg, Qt::Widget);
    if (!dlg->property("card").toBool())   // 메시지 카드는 둥근 모서리 밖을 비워 둠
        dlg->setAutoFillBackground(true);
    dlg->show();

    if (full) {
        dlg->setGeometry(bg.rect());
    } else {
        dlg->resize(dlg->size().expandedTo(dlg->sizeHint()).boundedTo(bg.size()));
        QPoint p = moved ? bg.mapFromGlobal(pos) : bg.rect().center() - dlg->rect().center();
        p.setX(qBound(0, p.x(), bg.width() - dlg->width()));
        p.setY(qBound(0, p.y(), bg.height() - dlg->height()));
        dlg->move(p);
    }
    dlg->setFocus();

    int ret = run(dlg);
    dlg->setParent(old, Qt::Dialog);
    return ret;
}

// 메시지 창 공통 처리: SNAP 디자인 카드 (동그란 아이콘 + 제목 + 내용 + 큰 버튼)
//  kind: 'i' 안내, 'w' 경고, 'q' 확인(예/아니오), 'c' 기다림(신호가 오면 자동으로 닫힘)
static int msg(char kind, QWidget *parent, const QString &title, const QString &text,
               const QString &okText, const QString &cancelText, QObject *src = 0, const char *signal = 0)
{
    QDialog dlg(parent);
    dlg.setObjectName("msgBox");
    dlg.setProperty("card", true);
    dlg.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dlg);
    outer->setContentsMargins(0, 0, 0, 0);
    QFrame *card = new QFrame;
    card->setObjectName("msgCard");
    outer->addWidget(card);

    QVBoxLayout *v = new QVBoxLayout(card);
    v->setContentsMargins(px(40), px(36), px(40), px(32));
    v->setSpacing(px(14));

    const char *symbol = kind == 'w' ? "!" : kind == 'q' ? "?" : kind == 'c' ? "…" : "✓";
    QLabel *icon = new QLabel(QString::fromUtf8(symbol));
    icon->setObjectName(kind == 'w' ? "msgIconWarn" : "msgIcon");
    icon->setFixedSize(px(72), px(72));
    icon->setAlignment(Qt::AlignCenter);
    v->addWidget(icon, 0, Qt::AlignHCenter);

    QLabel *lblTitle = new QLabel(title);
    lblTitle->setObjectName("msgTitle");
    lblTitle->setAlignment(Qt::AlignCenter);
    lblTitle->setWordWrap(true);
    v->addWidget(lblTitle);

    QLabel *lblText = new QLabel(text);
    lblText->setObjectName("msgText");
    lblText->setAlignment(Qt::AlignCenter);
    lblText->setWordWrap(true);
    v->addWidget(lblText);
    v->addSpacing(px(10));

    QHBoxLayout *buttons = new QHBoxLayout;
    buttons->setSpacing(px(12));
    if (!cancelText.isEmpty()) {
        QPushButton *cancel = new QPushButton(cancelText);
        cancel->setObjectName("msgCancel");
        QObject::connect(cancel, SIGNAL(clicked()), &dlg, SLOT(reject()));
        buttons->addWidget(cancel);
    }
    if (!okText.isEmpty()) {
        QPushButton *ok = new QPushButton(okText);
        ok->setObjectName("btnOk");
        ok->setDefault(true);
        QObject::connect(ok, SIGNAL(clicked()), &dlg, SLOT(accept()));
        buttons->addWidget(ok);
    }
    v->addLayout(buttons);

    dlg.setFixedWidth(px(600));
    if (src)
        QObject::connect(src, signal, &dlg, SLOT(accept()));
    return popup(&dlg);
}

namespace Msg {

// 안내 메시지
void info(QWidget *parent, const QString &title, const QString &text)
{
    msg('i', parent, title, text, "확인", QString());
}

// 경고 메시지
void warn(QWidget *parent, const QString &title, const QString &text)
{
    msg('w', parent, title, text, "확인", QString());
}

// 예/아니오 확인, 예면 true
bool ask(QWidget *parent, const QString &title, const QString &text)
{
    return msg('q', parent, title, text, "예", "아니오") == QDialog::Accepted;
}

// signal 이 올 때까지 기다리는 메시지 (예: 카드 태그 대기), 취소하면 false
bool wait(QWidget *parent, const QString &title, const QString &text, QObject *src, const char *signal)
{
    return msg('c', parent, title, text, QString(), "취소", src, signal) == QDialog::Accepted;
}

}
