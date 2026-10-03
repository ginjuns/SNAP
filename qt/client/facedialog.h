#ifndef FACEDIALOG_H
#define FACEDIALOG_H

#include <QDialog>

class QTimer;

namespace Ui {
class FaceDialog;
}

// 카메라로 얼굴을 인식해 사용자 ID를 돌려주는 창.
// 인식 성공 시 accept(), userId()로 결과 확인.
// allowPassword면 카메라 아래에 [비밀번호로 로그인] 버튼을 두어 대신 로그인할 수 있다.
class FaceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit FaceDialog(const QString &title, QWidget *parent = 0, bool allowPassword = false);
    ~FaceDialog();

    int userId() const { return m_userId; }

private slots:
    void processFrame();     // OpenCV 모드: 카메라 프레임 처리
    void acceptManualId();   // 시뮬레이션 모드: 입력한 ID로 인식 처리
    void passwordLogin();

private:
    struct Camera;
    Ui::FaceDialog *ui;
    Camera *m_camera;
    QTimer *m_timer;
    int m_userId;
    int m_lastLabel;
    int m_hits;
    int m_ticks;
    bool m_allowPassword;
};

#endif // FACEDIALOG_H
