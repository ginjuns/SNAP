#include "facedialog.h"

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
QString g_initError;
bool g_initialized = false;

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

// faces/<사용자ID>/*.jpg 사진으로 LBPH 모델을 학습한다. (프로그램 실행 후 최초 1회)
bool initRecognizer()
{
    if (g_initialized)
        return g_initError.isEmpty();
    g_initialized = true;

    if (!g_cascade.load("haarcascade_frontalface_default.xml")) {
        g_initError = "haarcascade_frontalface_default.xml 파일을 찾을 수 없습니다.";
        return false;
    }

    std::vector<cv::Mat> images;
    std::vector<int> labels;
    QDir root("faces");
    foreach (const QString &dirName, root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        const int id = dirName.toInt(&ok);
        if (!ok)
            continue;
        QDir dir(root.filePath(dirName));
        const QStringList filters = QStringList() << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp";
        foreach (const QString &file, dir.entryList(filters, QDir::Files)) {
            cv::Mat gray = cv::imread(QFile::encodeName(dir.filePath(file)).constData(), cv::IMREAD_GRAYSCALE);
            if (gray.empty())
                continue;
            cv::equalizeHist(gray, gray);
            cv::Rect r;
            cv::Mat face = largestFace(gray, &r) ? gray(r) : gray;
            cv::Mat resized;
            cv::resize(face, resized, kFaceSize);
            images.push_back(resized);
            labels.push_back(id);
        }
    }
    if (images.empty()) {
        g_initError = "faces/<사용자ID>/ 폴더에 학습용 얼굴 사진이 없습니다.";
        return false;
    }

#if CV_VERSION_MAJOR == 3 && CV_VERSION_MINOR < 3
    g_model = cv::face::createLBPHFaceRecognizer();   // OpenCV 3.2 이하 (Ubuntu 18.04 기본)
#else
    g_model = cv::face::LBPHFaceRecognizer::create();
#endif
    g_model->train(images, labels);
    return true;
}

} // namespace

struct FaceDialog::Camera { cv::VideoCapture cap; };
#else
struct FaceDialog::Camera {};
#endif

FaceDialog::FaceDialog(const QString &title, QWidget *parent)
    : QDialog(parent), m_camera(0), m_idEdit(0), m_timer(new QTimer(this)),
      m_userId(-1), m_lastLabel(-1), m_hits(0), m_ticks(0)
{
    setWindowTitle(title);

    m_view = new QLabel;
    m_view->setFixedSize(640, 480);
    m_view->setAlignment(Qt::AlignCenter);
    m_view->setStyleSheet("background: black; color: white;");
    m_status = new QLabel("카메라를 정면으로 바라봐 주세요.");
    m_status->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_view);
    layout->addWidget(m_status);

#ifdef USE_OPENCV
    connect(m_timer, SIGNAL(timeout()), SLOT(processFrame()));
    if (!initRecognizer()) {
        m_status->setText(g_initError);
    } else {
        m_camera = new Camera;
        if (m_camera->cap.open(0))
            m_timer->start(50);
        else
            m_status->setText("카메라를 열 수 없습니다.");
    }
#else
    m_view->setText("OpenCV 없이 빌드됨 (얼굴인식 시뮬레이션 모드)");
    m_idEdit = new QLineEdit;
    m_idEdit->setPlaceholderText("인식된 것으로 처리할 사용자 ID (예: 1=관리자, 2=회원)");
    QPushButton *ok = new QPushButton("인식 완료");
    connect(ok, SIGNAL(clicked()), SLOT(acceptManualId()));
    connect(m_idEdit, SIGNAL(returnPressed()), SLOT(acceptManualId()));
    layout->addWidget(m_idEdit);
    layout->addWidget(ok);
#endif

    QPushButton *cancel = new QPushButton("취소");
    connect(cancel, SIGNAL(clicked()), SLOT(reject()));
    layout->addWidget(cancel);
}

FaceDialog::~FaceDialog()
{
    delete m_camera;   // 카메라 해제
}

void FaceDialog::processFrame()
{
#ifdef USE_OPENCV
    cv::Mat frame;
    if (!m_camera->cap.read(frame) || frame.empty())
        return;

    if (++m_ticks > kTimeoutTicks) {
        m_timer->stop();
        QMessageBox::warning(this, windowTitle(), "얼굴을 인식하지 못했습니다. 다시 시도해 주세요.");
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
        m_status->setText(known ? QString("인식 중... (%1/%2)").arg(m_hits).arg(kRequiredHits)
                                : QString("등록되지 않은 얼굴입니다."));
    } else {
        m_hits = 0;
        m_status->setText("얼굴이 보이지 않습니다. 카메라를 정면으로 바라봐 주세요.");
    }

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    QImage image(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888);
    m_view->setPixmap(QPixmap::fromImage(image).scaled(m_view->size(), Qt::KeepAspectRatio));

    if (m_hits >= kRequiredHits) {
        m_timer->stop();
        m_userId = m_lastLabel;
        accept();
    }
#endif
}

void FaceDialog::acceptManualId()
{
    bool ok = false;
    const int id = m_idEdit ? m_idEdit->text().toInt(&ok) : 0;
    if (!ok)
        return;
    m_userId = id;
    accept();
}
