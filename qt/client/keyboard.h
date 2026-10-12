#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <QDialog>
#include <QList>

class QLineEdit;
class QPushButton;

namespace Ui {
class Keyboard;
}

class Keyboard : public QDialog
{
    Q_OBJECT
public:
    enum Mode { Text, English, Number };

    Keyboard(const QString &text, Mode mode, const QString &title, QWidget *parent = 0);
    ~Keyboard();
    QString text() const { return m_text; }

    static void attach(QLineEdit *edit, Mode mode, const QString &title);
    static bool open(QLineEdit *edit, Mode mode, const QString &title);

private slots:
    void onKey();
    void onBack();
    void onClear();
    void onSpace();
    void onShift();
    void onLang();

private:
    void relabel();
    void jamo(const QChar &j);
    void append(const QChar &c, bool comp);
    void replaceLast(const QChar &c);
    void refresh();

    Ui::Keyboard *ui;
    Mode m_mode;
    QString m_text;
    bool m_comp;
    bool m_kor;
    bool m_shift;
    QList<QPushButton *> m_keys;
    int m_max;
};

#endif
