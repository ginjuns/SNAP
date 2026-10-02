#include "kioskdialog.h"

#include "qtcompat.h"

int execInWindow(QDialog *dialog, bool fill)
{
    QWidget *oldParent = dialog->parentWidget();
    QWidget *top = oldParent ? oldParent->window() : qApp->activeWindow();
    if (!top)
        return dialog->exec();

    // 지금 위치(전역 좌표)는 창 안 좌표로 바꿔서 쓴다. (가상 키보드가 입력칸 옆에 뜨도록 move() 해 둔 경우)
    const bool moved = dialog->testAttribute(Qt::WA_Moved);
    const QPoint globalPos = dialog->pos();

    QWidget backdrop(top);
    backdrop.setObjectName("kioskBackdrop");   // 스타일이 다이얼로그 안 위젯까지 번지지 않게 이름으로 지정
    backdrop.setAttribute(Qt::WA_StyledBackground);
    backdrop.setStyleSheet("QWidget#kioskBackdrop { background: rgba(0, 0, 0, 110); }");
    backdrop.setGeometry(top->rect());
    backdrop.show();
    backdrop.raise();

    dialog->setParent(&backdrop, Qt::Widget);
    dialog->setAutoFillBackground(true);
    dialog->show();   // QMessageBox는 보여질 때 크기가 정해지므로 먼저 띄우고 위치를 잡는다

    if (fill) {
        dialog->setGeometry(backdrop.rect());
    } else {
        dialog->resize(dialog->size().boundedTo(backdrop.size()));
        QPoint pos = moved ? backdrop.mapFromGlobal(globalPos)
                           : backdrop.rect().center() - dialog->rect().center();
        pos.setX(qBound(0, pos.x(), backdrop.width() - dialog->width()));
        pos.setY(qBound(0, pos.y(), backdrop.height() - dialog->height()));
        dialog->move(pos);
    }
    dialog->setFocus();

    const int result = dialog->exec();
    dialog->setParent(oldParent, Qt::Dialog);   // backdrop이 지워질 때 다이얼로그까지 지워지지 않게 떼어 놓는다
    return result;
}

static QMessageBox::StandardButton message(QMessageBox::Icon icon, QWidget *parent, const QString &title,
                                           const QString &text, QMessageBox::StandardButtons buttons)
{
    // 제목 표시줄이 없으므로 제목을 굵은 큰 글씨로, 내용은 그 아래에 보여준다.
    QMessageBox box(icon, title, title, buttons, parent);
    box.setInformativeText(text);
    box.setStyleSheet(css("QLabel#qt_msgbox_label { font-size: 20px; font-weight: bold; }"));
    return QMessageBox::StandardButton(execInWindow(&box));
}

namespace KioskMessage {

QMessageBox::StandardButton information(QWidget *parent, const QString &title, const QString &text,
                                        QMessageBox::StandardButtons buttons)
{
    return message(QMessageBox::Information, parent, title, text, buttons);
}

QMessageBox::StandardButton warning(QWidget *parent, const QString &title, const QString &text,
                                    QMessageBox::StandardButtons buttons)
{
    return message(QMessageBox::Warning, parent, title, text, buttons);
}

QMessageBox::StandardButton question(QWidget *parent, const QString &title, const QString &text,
                                     QMessageBox::StandardButtons buttons)
{
    return message(QMessageBox::Question, parent, title, text, buttons);
}

} // namespace KioskMessage
