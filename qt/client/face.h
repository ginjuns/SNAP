// 얼굴인식 화면: 카메라로 회원 얼굴을 찾아 로그인/결제 확인 (face.cpp 참고)

#ifndef FACE_H
#define FACE_H

#include <QDialog>

class QTimer;

namespace Ui {
class Face;
}

class Face : public QDialog
{
    Q_OBJECT
public:
    explicit Face(const QString &title, QWidget *parent = 0, bool pw = false);
    ~Face();

    int id() const { return m_id; }
    bool byPassword() const { return m_byPw; }

private slots:
    void tick();
    void pwLogin();

private:
    struct Cam;
    Ui::Face *ui;
    Cam *m_cam;
    QTimer *m_timer;
    int m_id;
    int m_last;
    int m_hits;
    int m_ticks;
    bool m_pw;
    bool m_byPw;
};

#endif
