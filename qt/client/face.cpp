#include "face.h"
#include "ui_face.h"
#include "login.h"
#include "net.h"
#include "popup.h"
#include "util.h"

#ifdef USE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>

namespace {

const double MAX_DIST = 80.0;
const int HITS = 5;
const int MAX_TICKS = 400;
const cv::Size FACE_SIZE(200, 200);

cv::CascadeClassifier cascade;
cv::Ptr<cv::face::LBPHFaceRecognizer> model;
int lastId = 0;

bool findFace(const cv::Mat &gray, cv::Rect *r)
{
    std::vector<cv::Rect> faces;
    cascade.detectMultiScale(gray, faces, 1.2, 5, 0, cv::Size(80, 80));
    if (faces.empty())
        return false;
    *r = faces[0];
    for (size_t i = 1; i < faces.size(); ++i)
        if (faces[i].area() > r->area())
            *r = faces[i];
    return true;
}

bool loadCascade(QString *err)
{
    if (!cascade.empty())
        return true;
    const char *paths[] = { "haarcascade_frontalface_default.xml",
                            "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
                            "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml" };
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
        if (cascade.load(paths[i]))
            return true;
    *err = "haarcascade_frontalface_default.xml 파일을 찾을 수 없습니다.\n"
           "Ubuntu: sudo apt install opencv-data";
    return false;
}

bool train(QString *err)
{
    if (!loadCascade(err))
        return false;

    QVariant data;
    if (!Net::call("FACES", QVariantMap{{"afterId", lastId}}, &data, err))
        return false;

    std::vector<cv::Mat> imgs;
    std::vector<int> labels;
    foreach (const QVariant &v, data.toList()) {
        QVariantMap m = v.toMap();
        lastId = m.value("id").toInt();
        QByteArray bytes = QByteArray::fromBase64(m.value("image").toString().toLatin1());
        std::vector<uchar> buf(bytes.begin(), bytes.end());
        cv::Mat gray = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
        if (gray.empty())
            continue;
        if (gray.cols > 800)
            cv::resize(gray, gray, cv::Size(800, gray.rows * 800 / gray.cols));
        cv::equalizeHist(gray, gray);
        cv::Rect r;
        if (!findFace(gray, &r))
            continue;
        cv::Mat roi;
        cv::resize(gray(r), roi, FACE_SIZE);
        imgs.push_back(roi);
        labels.push_back(m.value("userId").toInt());
    }

    if (!imgs.empty()) {
        if (model.empty()) {
#if CV_VERSION_MAJOR == 3 && CV_VERSION_MINOR < 3
            model = cv::face::createLBPHFaceRecognizer();
#else
            model = cv::face::LBPHFaceRecognizer::create();
#endif
            model->train(imgs, labels);
        } else {
            model->update(imgs, labels);
        }
    }
    if (model.empty()) {
        *err = "등록된 얼굴이 없습니다. 앱에서 얼굴 사진으로 회원가입을 먼저 해 주세요.";
        return false;
    }
    return true;
}

}

struct Face::Cam { cv::VideoCapture cap; };
#else
struct Face::Cam {};
#endif

Face::Face(const QString &title, QWidget *parent, bool pw)
    : QDialog(parent), ui(new Ui::Face), m_cam(0), m_timer(new QTimer(this)),
      m_id(-1), m_last(-1), m_hits(0), m_ticks(0), m_pw(pw)
{
    ui->setupUi(this);
    scaleUi(this);
    setWindowTitle(title);
    ui->lblTitle->setText(title);

    ui->btnPw->setVisible(pw);
    connect(ui->btnPw, SIGNAL(clicked()), SLOT(pwLogin()));
    connect(ui->btnCancel, SIGNAL(clicked()), SLOT(reject()));

#ifdef USE_OPENCV
    connect(m_timer, SIGNAL(timeout()), SLOT(tick()));
    QString err;
    if (!train(&err)) {
        ui->lblStatus->setText(err);
    } else {
        m_cam = new Cam;
        if (m_cam->cap.open(0))
            m_timer->start(50);
        else
            ui->lblStatus->setText("카메라를 열 수 없습니다.");
    }
#else
    ui->lblStatus->setText("OpenCV 없이 빌드되어 얼굴인식을 사용할 수 없습니다.");
#endif
}

Face::~Face()
{
    delete m_cam;
    delete ui;
}

void Face::tick()
{
#ifdef USE_OPENCV
    cv::Mat img;
    if (!m_cam->cap.read(img) || img.empty())
        return;

    if (++m_ticks > MAX_TICKS) {
        m_timer->stop();
        if (m_pw) {
            ui->lblStatus->setText("얼굴을 인식하지 못했습니다. [비밀번호로 로그인]을 눌러 주세요.");
            return;
        }
        Msg::warn(this, windowTitle(), "얼굴을 인식하지 못했습니다. 다시 시도해 주세요.");
        reject();
        return;
    }

    cv::Mat gray;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::equalizeHist(gray, gray);

    cv::Rect r;
    if (findFace(gray, &r)) {
        cv::Mat roi;
        cv::resize(gray(r), roi, FACE_SIZE);
        int label = -1;
        double dist = 0;
        model->predict(roi, label, dist);

        bool known = dist < MAX_DIST;
        cv::rectangle(img, r, known ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);
        if (known && label == m_last) {
            ++m_hits;
        } else {
            m_last = known ? label : -1;
            m_hits = known ? 1 : 0;
        }
        ui->lblStatus->setText(known ? QString("인식 중... (%1/%2)").arg(m_hits).arg(HITS)
                                     : QString("등록되지 않은 얼굴입니다."));
    } else {
        m_hits = 0;
        ui->lblStatus->setText("얼굴이 보이지 않습니다. 카메라를 정면으로 바라봐 주세요.");
    }

    cv::Mat rgb;
    cv::cvtColor(img, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, int(rgb.step), QImage::Format_RGB888);
    ui->lblCam->setPixmap(QPixmap::fromImage(qimg).scaled(ui->lblCam->size(), Qt::KeepAspectRatio));

    if (m_hits >= HITS) {
        m_timer->stop();
        m_id = m_last;
        accept();
    }
#endif
}

void Face::pwLogin()
{
    bool running = m_timer->isActive();
    m_timer->stop();

    Login dlg(this);
    if (popup(&dlg) == QDialog::Accepted) {
        m_id = dlg.id();
        accept();
    } else if (running) {
        m_ticks = 0;
        m_timer->start(50);
    }
}
