#include "keyboard.h"
#include "ui_keyboard.h"
#include "popup.h"
#include "util.h"

namespace {

const QString CHO  = QString::fromUtf8("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ");
const QString JUNG = QString::fromUtf8("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ");
const QString JONG = QString::fromUtf8(" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ");

const QStringList JONG2 = QString::fromUtf8("ㄱㅅㄳ ㄴㅈㄵ ㄴㅎㄶ ㄹㄱㄺ ㄹㅁㄻ ㄹㅂㄼ ㄹㅅㄽ ㄹㅌㄾ ㄹㅍㄿ ㄹㅎㅀ ㅂㅅㅄ").split(' ');
const QStringList JUNG2 = QString::fromUtf8("ㅗㅏㅘ ㅗㅐㅙ ㅗㅣㅚ ㅜㅓㅝ ㅜㅔㅞ ㅜㅣㅟ ㅡㅣㅢ").split(' ');

const char *KO[2][3] = {
    { "ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔ", "ㅁㄴㅇㄹㅎㅗㅓㅏㅣ", "ㅋㅌㅊㅍㅠㅜㅡ" },
    { "ㅃㅉㄸㄲㅆㅛㅕㅑㅒㅖ", "ㅁㄴㅇㄹㅎㅗㅓㅏㅣ", "ㅋㅌㅊㅍㅠㅜㅡ" }
};
const char *EN[2][3] = {
    { "qwertyuiop", "asdfghjkl", "zxcvbnm" },
    { "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM" }
};

QChar combine(const QChar &a, const QChar &b, const QStringList &table)
{
    foreach (const QString &t, table)
        if (t.at(0) == a && t.at(1) == b)
            return t.at(2);
    return QChar();
}

bool split(const QChar &c, const QStringList &table, QChar *a, QChar *b)
{
    foreach (const QString &t, table)
        if (t.at(2) == c) {
            *a = t.at(0);
            *b = t.at(1);
            return true;
        }
    return false;
}

QChar compose(int cho, int jung, int jong)
{
    return QChar(0xAC00 + (cho * 21 + jung) * 28 + jong);
}

bool decompose(const QChar &c, int *cho, int *jung, int *jong)
{
    int code = c.unicode() - 0xAC00;
    if (code < 0 || code >= 11172)
        return false;
    *cho = code / (21 * 28);
    *jung = (code % (21 * 28)) / 28;
    *jong = code % 28;
    return true;
}

class Opener : public QObject
{
public:
    Opener(QLineEdit *edit, Keyboard::Mode mode, const QString &title)
        : QObject(edit), m_edit(edit), m_mode(mode), m_title(title) {}

protected:
    bool eventFilter(QObject *, QEvent *e)
    {
        if (e->type() != QEvent::MouseButtonRelease || !m_edit->isEnabled())
            return false;
        Keyboard::open(m_edit, m_mode, m_title);
        return true;
    }

private:
    QLineEdit *m_edit;
    Keyboard::Mode m_mode;
    QString m_title;
};

}

void Keyboard::attach(QLineEdit *edit, Mode mode, const QString &title)
{
    edit->installEventFilter(new Opener(edit, mode, title));
}

bool Keyboard::open(QLineEdit *edit, Mode mode, const QString &title)
{
    Keyboard kb(edit->text(), mode, title, edit->window());
    kb.ui->edit->setEchoMode(edit->echoMode());

    QRect screen = QApplication::desktop()->availableGeometry(edit);
    QRect field(edit->mapToGlobal(QPoint(0, 0)), edit->size());
    QSize size = kb.sizeHint();
    int y = field.bottom() + 8;
    if (y + size.height() > screen.bottom())
        y = field.top() - 8 - size.height();
    y = qBound(screen.top(), y, screen.bottom() - size.height());
    int x = qBound(screen.left(), field.center().x() - size.width() / 2, screen.right() - size.width());
    kb.move(x, y);

    if (popup(&kb) != QDialog::Accepted)
        return false;
    edit->setText(kb.text());
    return true;
}

Keyboard::Keyboard(const QString &text, Mode mode, const QString &title, QWidget *parent)
    : QDialog(parent), ui(new Ui::Keyboard), m_mode(mode), m_text(text), m_comp(false),
      m_kor(mode != English), m_shift(false), m_max(mode == Number ? 9 : 30)
{
    ui->setupUi(this);
    scaleUi(this);
    setWindowTitle(title);
    ui->lblTitle->setText(title);

    int key = (mode == Number) ? 56 : 42;
    setStyleSheet(css(QString("QPushButton { font-size: %1px; min-width: %2px; min-height: %2px; padding: 0 6px; }"
                              "QLineEdit { font-size: 20px; padding: 4px; }")
                          .arg(mode == Number ? 20 : 16).arg(key)));
    foreach (QLayout *l, findChildren<QLayout *>())
        l->setSpacing(px(4));
    layout()->setContentsMargins(px(8), px(8), px(8), px(8));

    foreach (QPushButton *b, findChildren<QPushButton *>(QRegularExpression("^[ndk]\\d+$")))
        connect(b, SIGNAL(clicked()), SLOT(onKey()));
    connect(ui->nClear, SIGNAL(clicked()), SLOT(onClear()));
    connect(ui->nBack, SIGNAL(clicked()), SLOT(onBack()));
    connect(ui->btnShift, SIGNAL(clicked()), SLOT(onShift()));
    connect(ui->btnBack, SIGNAL(clicked()), SLOT(onBack()));
    connect(ui->btnLang, SIGNAL(clicked()), SLOT(onLang()));
    connect(ui->btnSpace, SIGNAL(clicked()), SLOT(onSpace()));
    connect(ui->btnClear, SIGNAL(clicked()), SLOT(onClear()));
    connect(ui->btnCancel, SIGNAL(clicked()), SLOT(reject()));
    connect(ui->btnOk, SIGNAL(clicked()), SLOT(accept()));

    if (mode == Number) {
        ui->textPad->hide();
    } else {
        ui->numPad->hide();
        for (int i = 0; i < 26; ++i)
            m_keys << findChild<QPushButton *>(QString("k%1").arg(i));
        relabel();
    }
    adjustSize();

    refresh();
}

Keyboard::~Keyboard()
{
    delete ui;
}

void Keyboard::relabel()
{
    const char **rows = m_kor ? KO[m_shift ? 1 : 0] : EN[m_shift ? 1 : 0];
    int k = 0;
    for (int r = 0; r < 3; ++r) {
        QString s = QString::fromUtf8(rows[r]);
        for (int i = 0; i < s.size(); ++i)
            m_keys[k++]->setText(s.at(i));
    }
    ui->btnShift->setChecked(m_shift);
    ui->btnLang->setText(m_kor ? "한 → 영" : "영 → 한");
}

void Keyboard::refresh()
{
    ui->edit->setText(m_text);
    ui->edit->setCursorPosition(m_text.size());
}

void Keyboard::append(const QChar &c, bool comp)
{
    if (m_text.size() >= m_max)
        return;
    m_text.append(c);
    m_comp = comp;
}

void Keyboard::replaceLast(const QChar &c)
{
    m_text[m_text.size() - 1] = c;
    m_comp = true;
}

void Keyboard::onKey()
{
    QChar c = qobject_cast<QPushButton *>(sender())->text().at(0);
    if (m_mode != Number && m_kor && (CHO.contains(c) || JUNG.contains(c)))
        jamo(c);
    else
        append(c, false);

    if (m_shift) {
        m_shift = false;
        relabel();
    }
    refresh();
}

void Keyboard::jamo(const QChar &j)
{
    QChar last = (m_comp && !m_text.isEmpty()) ? m_text.at(m_text.size() - 1) : QChar();
    int cho, jung, jong;

    if (!JUNG.contains(j)) {
        if (decompose(last, &cho, &jung, &jong)) {
            if (jong == 0 && JONG.indexOf(j) > 0) {
                replaceLast(compose(cho, jung, JONG.indexOf(j)));
                return;
            }
            QChar m = jong ? combine(JONG.at(jong), j, JONG2) : QChar();
            if (!m.isNull()) {
                replaceLast(compose(cho, jung, JONG.indexOf(m)));
                return;
            }
        }
        append(j, true);
        return;
    }

    if (!last.isNull() && CHO.contains(last)) {
        replaceLast(compose(CHO.indexOf(last), JUNG.indexOf(j), 0));
        return;
    }
    if (decompose(last, &cho, &jung, &jong)) {
        if (jong == 0) {
            QChar m = combine(JUNG.at(jung), j, JUNG2);
            if (!m.isNull()) {
                replaceLast(compose(cho, JUNG.indexOf(m), 0));
                return;
            }
        } else {
            QChar a, b;
            if (split(JONG.at(jong), JONG2, &a, &b)) {
                replaceLast(compose(cho, jung, JONG.indexOf(a)));
                append(compose(CHO.indexOf(b), JUNG.indexOf(j), 0), true);
                return;
            }
            QChar moved = JONG.at(jong);
            if (CHO.contains(moved)) {
                replaceLast(compose(cho, jung, 0));
                append(compose(CHO.indexOf(moved), JUNG.indexOf(j), 0), true);
                return;
            }
        }
    } else if (!last.isNull() && JUNG.contains(last)) {
        QChar m = combine(last, j, JUNG2);
        if (!m.isNull()) {
            replaceLast(m);
            return;
        }
    }
    append(j, true);
}

void Keyboard::onBack()
{
    if (m_text.isEmpty())
        return;
    int cho, jung, jong;
    QChar last = m_text.at(m_text.size() - 1);
    if (m_comp && decompose(last, &cho, &jung, &jong)) {
        QChar a, b;
        if (jong)
            replaceLast(compose(cho, jung, split(JONG.at(jong), JONG2, &a, &b) ? JONG.indexOf(a) : 0));
        else if (split(JUNG.at(jung), JUNG2, &a, &b))
            replaceLast(compose(cho, JUNG.indexOf(a), 0));
        else
            replaceLast(CHO.at(cho));
    } else {
        m_text.chop(1);
    }
    refresh();
}

void Keyboard::onClear()
{
    m_text.clear();
    m_comp = false;
    refresh();
}

void Keyboard::onSpace()
{
    append(' ', false);
    refresh();
}

void Keyboard::onShift()
{
    m_shift = !m_shift;
    relabel();
}

void Keyboard::onLang()
{
    m_kor = !m_kor;
    m_shift = false;
    m_comp = false;
    relabel();
}
