#ifndef VIRTUALKEYBOARD_H
#define VIRTUALKEYBOARD_H

#include <QDialog>
#include <QList>

class QLineEdit;
class QPushButton;

// 터치스크린용 가상 키보드.
//   Text   : 한글(두벌식, 자모 조합) / 영문 / 숫자
//   English: Text와 같지만 영문으로 시작 (아이디, 비밀번호)
//   Number : 숫자 키패드
// 사용법: VirtualKeyboard::attach(lineEdit, VirtualKeyboard::Text, "상품명");
//         -> 입력칸을 터치하면 키보드 창이 뜨고 [완료] 시 입력칸에 반영된다.
//         입력칸이 비밀번호(QLineEdit::Password)면 키보드 창에서도 ●로 가려진다.
class VirtualKeyboard : public QDialog
{
    Q_OBJECT
public:
    enum Mode { Text, English, Number };

    VirtualKeyboard(const QString &text, Mode mode, const QString &title, QWidget *parent = 0);
    QString text() const { return m_text; }

    static void attach(QLineEdit *edit, Mode mode, const QString &title);
    // 키보드 창을 바로 띄운다. [완료]를 누르면 입력칸에 반영하고 true.
    static bool open(QLineEdit *edit, Mode mode, const QString &title);

private slots:
    void onKey();
    void onBackspace();
    void onClear();
    void onSpace();
    void onShift();
    void onToggleLanguage();

private:
    QPushButton *makeKey(const QString &label, const char *slot);
    void relabel();
    void typeJamo(const QChar &jamo);
    void append(const QChar &c, bool composing);
    void replaceLast(const QChar &c);
    void refresh();

    Mode m_mode;
    QString m_text;
    bool m_composing;   // 마지막 글자가 조합 중인 한글인지
    bool m_korean;
    bool m_shift;
    QLineEdit *m_display;
    QList<QPushButton *> m_letterKeys;
    QPushButton *m_shiftKey;
    QPushButton *m_langKey;
    int m_maxLength;
};

#endif // VIRTUALKEYBOARD_H
