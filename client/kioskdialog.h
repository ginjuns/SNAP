#ifndef KIOSKDIALOG_H
#define KIOSKDIALOG_H

#include <QMessageBox>

class QDialog;

// 다이얼로그를 별도 창이 아니라 키오스크 화면(전체 화면 창) 안에 띄운다.
// 별도 창을 띄우면 Ubuntu 상단바/왼쪽 독이 다시 보이기 때문이다.
// 뒤는 어둡게 덮어서 다이얼로그를 닫기 전에는 다른 곳을 누를 수 없다.
//   fill = true  : 화면 전체를 채운다 (카메라 화면)
//   그 외        : 다이얼로그 크기 그대로 가운데 (move()로 위치를 정했으면 그 위치)
int execInWindow(QDialog *dialog, bool fill = false);

// QMessageBox::information / warning / question 과 같은 사용법. 화면 안에 띄운다.
namespace KioskMessage {
QMessageBox::StandardButton information(QWidget *parent, const QString &title, const QString &text,
                                        QMessageBox::StandardButtons buttons = QMessageBox::Ok);
QMessageBox::StandardButton warning(QWidget *parent, const QString &title, const QString &text,
                                    QMessageBox::StandardButtons buttons = QMessageBox::Ok);
QMessageBox::StandardButton question(QWidget *parent, const QString &title, const QString &text,
                                     QMessageBox::StandardButtons buttons = QMessageBox::Yes | QMessageBox::No);
}

#endif // KIOSKDIALOG_H
