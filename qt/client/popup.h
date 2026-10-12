#ifndef POPUP_H
#define POPUP_H

#include <QDialog>

int popup(QDialog *dlg, bool full = false);
void setBlocked(bool on);
void closeAll();

namespace Msg {
void info(QWidget *parent, const QString &title, const QString &text);
void warn(QWidget *parent, const QString &title, const QString &text);
bool ask(QWidget *parent, const QString &title, const QString &text);
bool wait(QWidget *parent, const QString &title, const QString &text, QObject *src, const char *signal);
}

#endif
