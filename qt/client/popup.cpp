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
    dlg->setAutoFillBackground(true);
    dlg->show();

    if (full) {
        dlg->setGeometry(bg.rect());
    } else {
        dlg->resize(dlg->size().boundedTo(bg.size()));
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

// 메시지 창 공통 처리 (제목 강조, 취소 버튼 한글화, 신호가 오면 자동으로 닫힘)
static int msg(QMessageBox::Icon icon, QWidget *parent, const QString &title, const QString &text,
               QMessageBox::StandardButtons buttons, QObject *src = 0, const char *signal = 0)
{
    QMessageBox box(icon, title, title, buttons, parent);
    box.setInformativeText(text);
    box.setStyleSheet(css("QLabel#qt_msgbox_label { font-size: 20px; font-weight: bold; }"));
    if (box.button(QMessageBox::Cancel))
        box.button(QMessageBox::Cancel)->setText("취소");
    if (src)
        QObject::connect(src, signal, &box, SLOT(accept()));
    return popup(&box);
}

namespace Msg {

// 안내 메시지
void info(QWidget *parent, const QString &title, const QString &text)
{
    msg(QMessageBox::Information, parent, title, text, QMessageBox::Ok);
}

// 경고 메시지
void warn(QWidget *parent, const QString &title, const QString &text)
{
    msg(QMessageBox::Warning, parent, title, text, QMessageBox::Ok);
}

// 예/아니오 확인, 예면 true
bool ask(QWidget *parent, const QString &title, const QString &text)
{
    return msg(QMessageBox::Question, parent, title, text, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

// signal 이 올 때까지 기다리는 메시지 (예: 카드 태그 대기), 취소하면 false
bool wait(QWidget *parent, const QString &title, const QString &text, QObject *src, const char *signal)
{
    return msg(QMessageBox::Information, parent, title, text, QMessageBox::Cancel, src, signal) == QDialog::Accepted;
}

}
