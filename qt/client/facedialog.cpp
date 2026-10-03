#include "facedialog.h"
#include "ui_facedialog.h"
#include "passworddialog.h"
#include "serverclient.h"

#include "kioskdialog.h"
#include "qtcompat.h"

#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>

namespace {

const double kThreshold = 80.0;      // LBPH 거리. 작을수록 비슷한 얼굴
const int kRequiredHits = 5;         // 같은 사람으로 연속 인식되어야 하는 프레임 수
const int kTimeoutTicks = 400;       // 50ms * 400 = 20초
const cv::Size kFaceSize(200, 200);

cv::CascadeClassifier g_cascade;
cv::Ptr<cv::face::LBPHFaceRecognizer> g_model;
int g_lastFaceId = 0;                // 마지막으로 학습한 face_images.id

bool largestFace(const cv::Mat &gray, cv::Rect *face)
{
    std::vector<cv::Rect> faces;
    g_cascade.detectMultiScale(gray, faces, 1.2, 5, 0, cv::Size(80, 80));
    if (faces.empty())
        return false;
    *face = faces[0];
    for (size_t i = 1; i < faces.size(); ++i)
        if (faces[i].area() > face->area())
            *face = faces[i];
    return true;
}

// 실행 폴더에 없으면 Ubuntu 패키지(opencv-data) 위치에서 찾는다.
bool loadCascade(QString *err)
{
    if (!g_cascade.empty())
        return true;
    const char *paths[] = { "haarcascade_frontalface_default.xml",
                            "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
                            "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml" };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
        if (g_cascade.load(paths[i]))
            return true;
    *err = "haarcascade_frontalface_default.xml 파일을 찾을 수 없습니다.\n"
           "Ubuntu: sudo apt install opencv-data";
    return false;
}

// DB(face_images)의 얼굴 사진으로 LBPH 모델을 학습한다.
// 처음엔 전체를 받고, 이후 창을 열 때마다 새로 가입한 사람의 사진만 받아 모델에 더한다.
bool refreshRecognizer(QString *err)
{
    if (!loadCascade(err))
        return false;

    QVariant data;
    if (!ServerClient::call("FACES", QVariantMap{{"afterId", g_lastFaceId}}, &data, err))
        return false;

    std::vector<cv::Mat> images;
    std::vector<int> labels;
    foreach (const QVariant &v, data.toList()) {
        const QVariantMap m = v.toMap();
        g_lastFaceId = m.value("id").toInt();
        const QByteArray bytes = QByteArray::fromBase64(m.value("image").toString().toLatin1());
        const std::vector<uchar> buf(bytes.begin(), bytes.end());
        cv::Mat gray = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
        if (gray.empty())
            continue;
        if (gray.cols > 800)   // 폰 원본 사진은 너무 커서 얼굴 찾기가 느리다
            cv::resize(gray, gray, cv::Size(800, gray.rows * 800 / gray.cols));
        cv::equalizeHist(gray, gray);
        cv::Rect r;
        if (!largestFace(gray, &r))   // 얼굴이 안 보이는 사진은 학습에서 뺀다
            continue;
        cv::Mat resized;
        cv::resize(gray(r), resized, kFaceSize);
        images.push_back(resized);
        labels.push_back(m.value("userId").toInt());
    }

    if (!images.empty()) {
        if (g_model.empty()) {
#if CV_VERSION_MAJOR == 3 && CV_VERSION_MINOR < 3
            g_model = cv::face::createLBPHFaceRecognizer();   // OpenCV 3.2 이하 (Ubuntu 18.04 기본)
#else
            g_model = cv::face::LBPHFaceRecognizer::create();
#endif
            g_model->train(images, labels);
        } else {
            g_model->update(images, labels);
        }
    }
    if (g_model.empty()) {
        *err = "등록된 얼굴이 없습니다. 앱에서 얼굴 사진으로 회원가입을 먼저 해 주세요.";
        return false;
    }
    return true;
}

} // namespace

struct FaceDialog::Camera { cv::VideoCapture cap; };
#else
struct FaceDialog::Camera {};
#endif

FaceDialog::FaceDialog(const QString &title, QWidget *parent, bool allowPassword)
    : QDialog(parent), ui(new Ui::FaceDialog), m_camera(0), m_timer(new QTimer(this)),
      m_userId(-1), m_lastLabel(-1), m_hits(0), m_ticks(0), m_allowPassword(allowPassword)
{
    // 화면 배치: facedialog.ui. 내용은 화면 가운데 카메라 너비만큼의 세로 칸(panel)에 모은다.
    // execInWindow(dialog, true)로 키오스크 화면 전체를 덮어 띄운다.
    ui->setupUi(this);
    scaleUi(this);
    setWindowTitle(title);
    ui->captionLabel->setText(title);

    ui->passwordButton->setVisible(allowPassword);
    connect(ui->passwordButton, SIGNAL(clicked()), SLOT(passwordLogin()));
    connect(ui->cancelButton, SIGNAL(clicked()), SLOT(reject()));

#ifdef USE_OPENCV
    ui->idEdit->hide();     // 시뮬레이션 모드 전용
    ui->okButton->hide();
    connect(m_timer, SIGNAL(timeout()), SLOT(processFrame()));
    QString err;
    if (!refreshRecognizer(&err)) {
        ui->statusLabel->setText(err);
    } else {
        m_camera = new Camera;
        if (m_camera->cap.open(0))
            m_timer->start(50);
        else
            ui->statusLabel->setText("카메라를 열 수 없습니다.");
    }
#else
    ui->viewLabel->setText("OpenCV 없이 빌드됨 (얼굴인식 시뮬레이션 모드)");
    connect(ui->okButton, SIGNAL(clicked()), SLOT(acceptManualId()));
    connect(ui->idEdit, SIGNAL(returnPressed()), SLOT(acceptManualId()));
#endif
}

FaceDialog::~FaceDialog()
{
    delete m_camera;   // 카메라 해제
    delete ui;
}

void FaceDialog::processFrame()
{
#ifdef USE_OPENCV
    cv::Mat frame;
    if (!m_camera->cap.read(frame) || frame.empty())
        return;

    if (++m_ticks > kTimeoutTicks) {
        m_timer->stop();
        if (m_allowPassword) {   // 창을 닫지 않고 비밀번호 로그인을 안내
            ui->statusLabel->setText("얼굴을 인식하지 못했습니다. [비밀번호로 로그인]을 눌러 주세요.");
            return;
        }
        KioskMessage::warning(this, windowTitle(), "얼굴을 인식하지 못했습니다. 다시 시도해 주세요.");
        reject();
        return;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    cv::Rect r;
    if (largestFace(gray, &r)) {
        cv::Mat face;
        cv::resize(gray(r), face, kFaceSize);
        int label = -1;
        double distance = 0;
        g_model->predict(face, label, distance);

        const bool known = distance < kThreshold;
        cv::rectangle(frame, r, known ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);
        if (known && label == m_lastLabel) {
            ++m_hits;
        } else {
            m_lastLabel = known ? label : -1;
            m_hits = known ? 1 : 0;
        }
        ui->statusLabel->setText(known ? QString("인식 중... (%1/%2)").arg(m_hits).arg(kRequiredHits)
                                : QString("등록되지 않은 얼굴입니다."));
    } else {
        m_hits = 0;
        ui->statusLabel->setText("얼굴이 보이지 않습니다. 카메라를 정면으로 바라봐 주세요.");
    }

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    QImage image(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888);
    ui->viewLabel->setPixmap(QPixmap::fromImage(image).scaled(ui->viewLabel->size(), Qt::KeepAspectRatio));

    if (m_hits >= kRequiredHits) {
        m_timer->stop();
        m_userId = m_lastLabel;
        accept();
    }
#endif
}

void FaceDialog::passwordLogin()
{
    // 비밀번호를 입력하는 동안은 얼굴인식(과 20초 제한)을 멈춘다.
    const bool scanning = m_timer->isActive();
    m_timer->stop();

    PasswordDialog dialog(this);
    if (execInWindow(&dialog) == QDialog::Accepted) {
        m_userId = dialog.userId();
        accept();
    } else if (scanning) {
        m_ticks = 0;
        m_timer->start(50);
    }
}

void FaceDialog::acceptManualId()
{
    bool ok = false;
    const int id = ui->idEdit->text().toInt(&ok);
    if (!ok)
        return;
    m_userId = id;
    accept();
}
