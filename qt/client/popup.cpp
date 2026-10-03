#include "popup.h"
#include "util.h"

static bool blocked = false;
static QList<QDialog *> opened;

void setBlocked(bool on)
{
    blocked = on;
}

void closeAll()
{
    QList<QDialog *> list = opened;
    for (int i = list.size() - 1; i >= 0; --i)
        list.at(i)->reject();
}

static int run(QDialog *dlg)
{
    opened << dlg;
    int ret = dlg->exec();
    opened.removeAll(dlg);
    return ret;
}

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

void info(QWidget *parent, const QString &title, const QString &text)
{
    msg(QMessageBox::Information, parent, title, text, QMessageBox::Ok);
}

void warn(QWidget *parent, const QString &title, const QString &text)
{
    msg(QMessageBox::Warning, parent, title, text, QMessageBox::Ok);
}

bool ask(QWidget *parent, const QString &title, const QString &text)
{
    return msg(QMessageBox::Question, parent, title, text, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

bool wait(QWidget *parent, const QString &title, const QString &text, QObject *src, const char *signal)
{
    return msg(QMessageBox::Information, parent, title, text, QMessageBox::Cancel, src, signal) == QDialog::Accepted;
}

}
