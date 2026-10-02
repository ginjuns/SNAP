#include "virtualkeyboard.h"

#include "kioskdialog.h"
#include "qtcompat.h"

namespace {

// ---- 한글 조합 테이블 (유니코드 한글 음절 = 0xAC00 + (초성*21 + 중성)*28 + 종성) ----
const QString CHO  = QString::fromUtf8("ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ");
const QString JUNG = QString::fromUtf8("ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ");
const QString JONG = QString::fromUtf8(" ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ");   // 0번 = 종성 없음

// 겹받침 / 겹모음: "앞 + 뒤 = 결과"
const QStringList DOUBLE_JONG = QString::fromUtf8("ㄱㅅㄳ ㄴㅈㄵ ㄴㅎㄶ ㄹㄱㄺ ㄹㅁㄻ ㄹㅂㄼ ㄹㅅㄽ ㄹㅌㄾ ㄹㅍㄿ ㄹㅎㅀ ㅂㅅㅄ").split(' ');
const QStringList DOUBLE_VOWEL = QString::fromUtf8("ㅗㅏㅘ ㅗㅐㅙ ㅗㅣㅚ ㅜㅓㅝ ㅜㅔㅞ ㅜㅣㅟ ㅡㅣㅢ").split(' ');

// 두벌식 자판 (행별 글자 수: 10 / 9 / 7)
const char *KO_KEYS[2][3] = {
    { "ㅂㅈㄷㄱㅅㅛㅕㅑㅐㅔ", "ㅁㄴㅇㄹㅎㅗㅓㅏㅣ", "ㅋㅌㅊㅍㅠㅜㅡ" },
    { "ㅃㅉㄸㄲㅆㅛㅕㅑㅒㅖ", "ㅁㄴㅇㄹㅎㅗㅓㅏㅣ", "ㅋㅌㅊㅍㅠㅜㅡ" }
};
const char *EN_KEYS[2][3] = {
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

bool split(const QChar &c, const QStringList &table, QChar *first, QChar *second)
{
    foreach (const QString &t, table)
        if (t.at(2) == c) {
            *first = t.at(0);
            *second = t.at(1);
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
    const int code = c.unicode() - 0xAC00;
    if (code < 0 || code >= 11172)
        return false;
    *cho = code / (21 * 28);
    *jung = (code % (21 * 28)) / 28;
    *jong = code % 28;
    return true;
}

// 입력칸 터치 시 키보드를 띄우는 이벤트 필터
class KeyboardOpener : public QObject
{
public:
    KeyboardOpener(QLineEdit *edit, VirtualKeyboard::Mode mode, const QString &title)
        : QObject(edit), m_edit(edit), m_mode(mode), m_title(title) {}

protected:
    bool eventFilter(QObject *, QEvent *event)
    {
        if (event->type() != QEvent::MouseButtonRelease || !m_edit->isEnabled())
            return false;
        VirtualKeyboard::open(m_edit, m_mode, m_title);
        return true;
    }

private:
    QLineEdit *m_edit;
    VirtualKeyboard::Mode m_mode;
    QString m_title;
};

} // namespace

void VirtualKeyboard::attach(QLineEdit *edit, Mode mode, const QString &title)
{
    edit->installEventFilter(new KeyboardOpener(edit, mode, title));
}

bool VirtualKeyboard::open(QLineEdit *edit, Mode mode, const QString &title)
{
    VirtualKeyboard keyboard(edit->text(), mode, title, edit->window());
    keyboard.m_display->setEchoMode(edit->echoMode());

    // 키보드가 입력칸을 가리지 않도록 입력칸 바로 아래(자리가 없으면 위)에 띄운다.
    const QRect screen = QApplication::desktop()->availableGeometry(edit);
    const QRect field(edit->mapToGlobal(QPoint(0, 0)), edit->size());
    const QSize size = keyboard.sizeHint();
    int y = field.bottom() + 8;
    if (y + size.height() > screen.bottom())
        y = field.top() - 8 - size.height();
    y = qBound(screen.top(), y, screen.bottom() - size.height());
    const int x = qBound(screen.left(), field.center().x() - size.width() / 2, screen.right() - size.width());
    keyboard.move(x, y);

    if (execInWindow(&keyboard) != QDialog::Accepted)
        return false;
    edit->setText(keyboard.text());
    return true;
}

VirtualKeyboard::VirtualKeyboard(const QString &text, Mode mode, const QString &title, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_text(text), m_composing(false), m_korean(mode != English), m_shift(false),
      m_shiftKey(0), m_langKey(0), m_maxLength(mode == Number ? 9 : 30)
{
    setWindowTitle(title);
    // 앱 전체 스타일의 버튼 좌우 여백(16px)을 없애야 키가 작아진다.
    // 글자 키보드는 약 500x360px, 숫자 키패드는 조금 더 큰 키를 쓴다.
    const int keySize = (mode == Number) ? 56 : 42;
    setStyleSheet(QString("QPushButton { font-size: %1px; min-width: %2px; min-height: %2px; padding: 0 6px; }"
                          "QLineEdit { font-size: 20px; padding: 4px; }")
                      .arg(mode == Number ? 20 : 16).arg(keySize));

    m_display = new QLineEdit;
    m_display->setReadOnly(true);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(8, 8, 8, 8);
    QLabel *caption = new QLabel(title);
    caption->setStyleSheet("font-size: 15px; font-weight: bold;");
    layout->addWidget(caption);
    layout->addWidget(m_display);

    if (mode == Number) {
        QGridLayout *pad = new QGridLayout;
        for (int i = 1; i <= 9; ++i)
            pad->addWidget(makeKey(QString::number(i), SLOT(onKey())), (i - 1) / 3, (i - 1) % 3);
        pad->addWidget(makeKey("전체삭제", SLOT(onClear())), 3, 0);
        pad->addWidget(makeKey("0", SLOT(onKey())), 3, 1);
        pad->addWidget(makeKey("←", SLOT(onBackspace())), 3, 2);
        layout->addLayout(pad);
    } else {
        // 숫자 행
        QHBoxLayout *numbers = new QHBoxLayout;
        for (int i = 1; i <= 10; ++i)
            numbers->addWidget(makeKey(QString::number(i % 10), SLOT(onKey())));
        layout->addLayout(numbers);

        // 글자 3행 (라벨은 relabel()에서 한/영, Shift 상태에 따라 채움)
        const int counts[3] = { 10, 9, 7 };
        for (int row = 0; row < 3; ++row) {
            QHBoxLayout *line = new QHBoxLayout;
            if (row == 2) {
                m_shiftKey = makeKey("Shift", SLOT(onShift()));
                m_shiftKey->setCheckable(true);
                line->addWidget(m_shiftKey);
            }
            for (int i = 0; i < counts[row]; ++i) {
                QPushButton *key = makeKey(QString(), SLOT(onKey()));
                m_letterKeys << key;
                line->addWidget(key);
            }
            if (row == 2)
                line->addWidget(makeKey("←", SLOT(onBackspace())));
            layout->addLayout(line);
        }

        QHBoxLayout *bottom = new QHBoxLayout;
        m_langKey = makeKey("한/영", SLOT(onToggleLanguage()));
        QPushButton *space = makeKey("띄어쓰기", SLOT(onSpace()));
        bottom->addWidget(m_langKey);
        bottom->addWidget(space, 1);
        bottom->addWidget(makeKey("전체삭제", SLOT(onClear())));
        layout->addLayout(bottom);
        relabel();
    }

    QPushButton *cancel = new QPushButton("취소");
    QPushButton *done = new QPushButton("완료");
    done->setStyleSheet("background: #2a78d6; color: white; font-weight: bold;");
    connect(cancel, SIGNAL(clicked()), SLOT(reject()));
    connect(done, SIGNAL(clicked()), SLOT(accept()));
    QHBoxLayout *actions = new QHBoxLayout;
    actions->addWidget(cancel);
    actions->addWidget(done);
    layout->addLayout(actions);

    refresh();
}

QPushButton *VirtualKeyboard::makeKey(const QString &label, const char *slot)
{
    QPushButton *key = new QPushButton(label);
    key->setFocusPolicy(Qt::NoFocus);
    connect(key, SIGNAL(clicked()), slot);
    return key;
}

void VirtualKeyboard::relabel()
{
    const char **rows = m_korean ? KO_KEYS[m_shift ? 1 : 0] : EN_KEYS[m_shift ? 1 : 0];
    int k = 0;
    for (int row = 0; row < 3; ++row) {
        const QString letters = QString::fromUtf8(rows[row]);
        for (int i = 0; i < letters.size(); ++i)
            m_letterKeys[k++]->setText(letters.at(i));
    }
    m_shiftKey->setChecked(m_shift);
    m_langKey->setText(m_korean ? "한 → 영" : "영 → 한");
}

void VirtualKeyboard::refresh()
{
    m_display->setText(m_text);
    m_display->setCursorPosition(m_text.size());
}

void VirtualKeyboard::append(const QChar &c, bool composing)
{
    if (m_text.size() >= m_maxLength)
        return;
    m_text.append(c);
    m_composing = composing;
}

void VirtualKeyboard::replaceLast(const QChar &c)
{
    m_text[m_text.size() - 1] = c;
    m_composing = true;
}

void VirtualKeyboard::onKey()
{
    const QChar c = qobject_cast<QPushButton *>(sender())->text().at(0);
    if (m_mode != Number && m_korean && (CHO.contains(c) || JUNG.contains(c)))
        typeJamo(c);
    else
        append(c, false);

    if (m_shift) {   // Shift는 한 글자 입력 후 자동 해제
        m_shift = false;
        relabel();
    }
    refresh();
}

// 두벌식 오토마타: 마지막 글자(조합 중일 때만)에 자모를 합친다.
void VirtualKeyboard::typeJamo(const QChar &jamo)
{
    const QChar last = (m_composing && !m_text.isEmpty()) ? m_text.at(m_text.size() - 1) : QChar();
    int cho, jung, jong;

    if (!JUNG.contains(jamo)) {   // 자음 입력
        if (decompose(last, &cho, &jung, &jong)) {
            if (jong == 0 && JONG.indexOf(jamo) > 0) {                       // 가 + ㄱ = 각
                replaceLast(compose(cho, jung, JONG.indexOf(jamo)));
                return;
            }
            const QChar merged = jong ? combine(JONG.at(jong), jamo, DOUBLE_JONG) : QChar();
            if (!merged.isNull()) {                                          // 각 + ㅅ = 갃
                replaceLast(compose(cho, jung, JONG.indexOf(merged)));
                return;
            }
        }
        append(jamo, true);
        return;
    }

    // 모음 입력
    if (!last.isNull() && CHO.contains(last)) {                              // ㄱ + ㅏ = 가
        replaceLast(compose(CHO.indexOf(last), JUNG.indexOf(jamo), 0));
        return;
    }
    if (decompose(last, &cho, &jung, &jong)) {
        if (jong == 0) {
            const QChar merged = combine(JUNG.at(jung), jamo, DOUBLE_VOWEL);
            if (!merged.isNull()) {                                          // 고 + ㅏ = 과
                replaceLast(compose(cho, JUNG.indexOf(merged), 0));
                return;
            }
        } else {                                                             // 각 + ㅏ = 가가
            QChar first, second;
            if (split(JONG.at(jong), DOUBLE_JONG, &first, &second)) {        // 갃 + ㅏ = 각사
                replaceLast(compose(cho, jung, JONG.indexOf(first)));
                append(compose(CHO.indexOf(second), JUNG.indexOf(jamo), 0), true);
                return;
            }
            const QChar moved = JONG.at(jong);
            if (CHO.contains(moved)) {
                replaceLast(compose(cho, jung, 0));
                append(compose(CHO.indexOf(moved), JUNG.indexOf(jamo), 0), true);
                return;
            }
        }
    } else if (!last.isNull() && JUNG.contains(last)) {                      // ㅗ + ㅏ = ㅘ
        const QChar merged = combine(last, jamo, DOUBLE_VOWEL);
        if (!merged.isNull()) {
            replaceLast(merged);
            return;
        }
    }
    append(jamo, true);
}

// 조합 중인 글자는 자모 단위로 지운다. (각 -> 가 -> ㄱ)
void VirtualKeyboard::onBackspace()
{
    if (m_text.isEmpty())
        return;
    int cho, jung, jong;
    const QChar last = m_text.at(m_text.size() - 1);
    if (m_composing && decompose(last, &cho, &jung, &jong)) {
        QChar first, second;
        if (jong)
            replaceLast(compose(cho, jung, split(JONG.at(jong), DOUBLE_JONG, &first, &second) ? JONG.indexOf(first) : 0));
        else if (split(JUNG.at(jung), DOUBLE_VOWEL, &first, &second))
            replaceLast(compose(cho, JUNG.indexOf(first), 0));
        else
            replaceLast(CHO.at(cho));
    } else {
        m_text.chop(1);
    }
    refresh();
}

void VirtualKeyboard::onClear()
{
    m_text.clear();
    m_composing = false;
    refresh();
}

void VirtualKeyboard::onSpace()
{
    append(' ', false);
    refresh();
}

void VirtualKeyboard::onShift()
{
    m_shift = !m_shift;
    relabel();
}

void VirtualKeyboard::onToggleLanguage()
{
    m_korean = !m_korean;
    m_shift = false;
    m_composing = false;
    relabel();
}
