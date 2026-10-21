#include "face.h"
#include "ui_face.h"
#include "login.h"
#include "net.h"
#include "popup.h"
#include "util.h"

#ifdef USE_OPENCV

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/face.hpp>

#include <condition_variable>
#include <mutex>
#include <thread>

namespace {

const double MIN_COSINE = 0.6;  //0.7이랑 차이가 눈에 보임
const int HITS = 5;
const int MAX_TICKS = 600;

// 인식 구역: 화면 가운데 구역(가로/세로 비율) 안의, 화면 너비 대비 MIN_FACE_W 이상인 얼굴만 인식
const double ZONE_W = 0.5;
const double ZONE_H = 0.8;
const double MIN_FACE_W = 0.15;

// 위조 판별: 진짜 확률이 이 값보다 낮으면 사진/화면으로 봄 (실제 얼굴이 막히면 낮추고, 사진이 통과되면 높임)
const double REAL_MIN = 0.5;

enum FaceState
{
    FACE_NONE,
    FACE_OUTSIDE,
    FACE_TOO_FAR,
    FACE_OK
};

// 화면 가운데 인식 구역 사각형
cv::Rect zoneRect(const cv::Size &size)
{
    int w = cvRound(size.width * ZONE_W);
    int h = cvRound(size.height * ZONE_H);

    return cv::Rect((size.width - w) / 2, (size.height - h) / 2, w, h);
}

// 모델: YuNet(얼굴 검출), SFace(얼굴 비교), MiniFASNet(위조 판별)
const char *YUNET_MODEL = MODEL_DIR "/face_detection_yunet_2022mar.onnx";
const char *SFACE_MODEL = MODEL_DIR "/face_recognition_sface_2021dec.onnx";

cv::Ptr<cv::FaceDetectorYN> detector;
cv::Ptr<cv::FaceRecognizerSF> recognizer;

const char *FAS_V2_MODEL = MODEL_DIR "/MiniFASNetV2.onnx";
const char *FAS_V1SE_MODEL = MODEL_DIR "/MiniFASNetV1SE.onnx";

cv::dnn::Net fasV2;
cv::dnn::Net fasV1SE;

// 등록된 회원 얼굴의 특징 벡터
struct RegisteredFace
{
    int userId;
    cv::Mat feature;
};

std::vector<RegisteredFace> registeredFaces;

int lastId = 0;



// 얼굴 검출, 비교, 위조 판별 모델 로드
bool loadModels(QString *err)
{
    if (detector.empty())
    {
        try
        {
            detector = cv::FaceDetectorYN::create(
                YUNET_MODEL,
                "",
                cv::Size(320, 320),
                0.9f,
                0.3f,
                5000
            );
        }
        catch (const cv::Exception &e)
        {
            *err =
                QString("YuNet 모델을 불러올 수 없습니다.\n%1")
                    .arg(e.what());

            return false;
        }

        if (detector.empty())
        {
            *err =
                "YuNet 모델을 불러올 수 없습니다.\n"
                "face_detection_yunet_2022mar.onnx 파일을 확인해 주세요.";

            return false;
        }
    }
    if (recognizer.empty())
    {
        try
        {
            recognizer =
                cv::FaceRecognizerSF::create(
                    SFACE_MODEL,
                    ""
                );
        }
        catch (const cv::Exception &e)
        {
            *err =
                QString("SFace 모델을 불러올 수 없습니다.\n%1")
                    .arg(e.what());

            return false;
        }

        if (recognizer.empty())
        {
            *err =
                "SFace 모델을 불러올 수 없습니다.\n"
                "face_recognition_sface_2021dec.onnx 파일을 확인해 주세요.";

            return false;
        }
    }
    if (fasV2.empty() || fasV1SE.empty())
    {
        try
        {
            fasV2 = cv::dnn::readNetFromONNX(FAS_V2_MODEL);
            fasV1SE = cv::dnn::readNetFromONNX(FAS_V1SE_MODEL);
        }
        catch (const cv::Exception &e)
        {
            fasV2 = cv::dnn::Net();
            fasV1SE = cv::dnn::Net();
            *err =
                QString("위조 얼굴 판별 모델을 불러올 수 없습니다.\n"
                        "MiniFASNetV2.onnx, MiniFASNetV1SE.onnx 파일을 확인해 주세요.\n%1")
                    .arg(e.what());

            return false;
        }
    }

    return true;
}

// 위조 판별용: 얼굴 주변을 scale 배로 넓혀 80x80 으로 자름
cv::Mat fasCrop(const cv::Mat &img, const cv::Rect &box, double scale)
{
    scale = std::min(scale, (img.rows - 1) / double(box.height));
    scale = std::min(scale, (img.cols - 1) / double(box.width));

    double cx = box.x + box.width / 2.0;
    double cy = box.y + box.height / 2.0;
    double w = box.width * scale;
    double h = box.height * scale;

    int x1 = std::max(0, int(cx - w / 2));
    int y1 = std::max(0, int(cy - h / 2));
    int x2 = std::min(img.cols - 1, int(cx + w / 2));
    int y2 = std::min(img.rows - 1, int(cy + h / 2));

    cv::Mat out;
    cv::resize(img(cv::Rect(x1, y1, x2 - x1 + 1, y2 - y1 + 1)), out, cv::Size(80, 80));
    return out;
}

// 모델 하나의 진짜 확률 (출력: 인쇄 위조, 진짜, 화면 위조)
double fasReal(cv::dnn::Net &net, const cv::Mat &img, const cv::Rect &box, double scale)
{
    net.setInput(cv::dnn::blobFromImage(fasCrop(img, box, scale)));

    cv::Mat out = net.forward().reshape(1, 1).clone();
    double maxValue;
    cv::minMaxLoc(out, 0, &maxValue);
    cv::exp(out - maxValue, out);

    return out.at<float>(0, 1) / cv::sum(out)[0];
}

// 얼굴이 진짜일 확률 (V2 2.7배, V1SE 4배 크롭 결과의 평균)
double realScore(const cv::Mat &img, const cv::Rect &box)
{
    if (fasV2.empty() || fasV1SE.empty() || box.width <= 0 || box.height <= 0)
        return 0.0;

    try
    {
        return (fasReal(fasV2, img, box, 2.7) + fasReal(fasV1SE, img, box, 4.0)) / 2;
    }
    catch (const cv::Exception &)
    {
        return 0.0;
    }
}


// 이미지에서 얼굴 하나를 골라 검출 결과(한 줄 15개 값: 박스, 눈/코/입 위치, 점수)와 박스를 돌려줌
bool detectFace(
    const cv::Mat &img,
    cv::Mat *bestFace,
    cv::Rect *bestBox,
    bool useZone = false,
    FaceState *state = 0,
    std::vector<cv::Rect> *ignored = 0)
{
    if (state)
        *state = FACE_NONE;

    if (detector.empty() || img.empty())
        return false;
    try
    {
        detector->setInputSize(img.size());
    }
    catch (const cv::Exception &)
    {
        return false;
    }


    cv::Mat faces;

    try
    {
        detector->detect(
            img,
            faces
        );
    }
    catch (const cv::Exception &)
    {
        return false;
    }


    if (faces.empty() || faces.rows <= 0)
        return false;


    // 카메라 화면은 구역 안의 가장 큰 얼굴, 등록 사진은 점수가 가장 높은 얼굴
    cv::Rect zone = zoneRect(img.size());
    int minWidth = cvRound(img.cols * MIN_FACE_W);
    bool tooFar = false;

    int bestIndex = -1;
    float bestValue = 0;

    for (int i = 0; i < faces.rows; ++i)
    {
        float value = faces.at<float>(i, 14);

        if (useZone)
        {
            cv::Rect r(cvRound(faces.at<float>(i, 0)), cvRound(faces.at<float>(i, 1)),
                       cvRound(faces.at<float>(i, 2)), cvRound(faces.at<float>(i, 3)));
            cv::Point center(r.x + r.width / 2, r.y + r.height / 2);

            if (!zone.contains(center) || r.width < minWidth)
            {
                if (zone.contains(center))
                    tooFar = true;
                if (ignored)
                    ignored->push_back(r & cv::Rect(0, 0, img.cols, img.rows));
                continue;
            }
            value = float(r.width);
        }

        if (bestIndex < 0 || value > bestValue)
        {
            bestValue = value;
            bestIndex = i;
        }
    }

    if (bestIndex < 0)
    {
        if (state)
            *state = tooFar ? FACE_TOO_FAR : FACE_OUTSIDE;
        return false;
    }

    if (state)
        *state = FACE_OK;
    cv::Mat face =
        faces.row(bestIndex).clone();


    if (bestFace)
        *bestFace = face;
    if (bestBox)
    {
        int x =
            cvRound(face.at<float>(0, 0));

        int y =
            cvRound(face.at<float>(0, 1));

        int w =
            cvRound(face.at<float>(0, 2));

        int h =
            cvRound(face.at<float>(0, 3));


        cv::Rect box(
            x,
            y,
            w,
            h
        );

        box &=
            cv::Rect(
                0,
                0,
                img.cols,
                img.rows
            );


        *bestBox = box;
    }

    return true;
}

// 이미지에서 얼굴을 찾아 SFace 특징 벡터로 변환
bool extractFeature(
    const cv::Mat &img,
    cv::Mat *feature,
    cv::Rect *box,
    bool useZone = false,
    FaceState *state = 0,
    std::vector<cv::Rect> *ignored = 0)
{
    if (recognizer.empty())
        return false;


    cv::Mat face;

    if (!detectFace(
            img,
            &face,
            box,
            useZone,
            state,
            ignored))
    {
        return false;
    }


    cv::Mat aligned;


    try
    {
        recognizer->alignCrop(
            img,
            face,
            aligned
        );
        recognizer->feature(
            aligned,
            *feature
        );
    }
    catch (const cv::Exception &)
    {
        return false;
    }


    return
        feature &&
        !feature->empty();
}

// 서버에서 새로 등록된 얼굴 사진을 받아 특징 벡터로 저장
bool train(QString *err)
{
    if (!loadModels(err))
        return false;


    QVariant data;

    if (!Net::call(
            "FACES",
            QVariantMap{{"afterId", lastId}},
            &data,
            err))
    {
        return false;
    }


    foreach (
        const QVariant &v,
        data.toList())
    {
        QVariantMap m =
            v.toMap();


        lastId =
            m.value("id").toInt();


        int userId =
            m.value("userId").toInt();

        QByteArray bytes =
            QByteArray::fromBase64(
                m.value("image")
                    .toString()
                    .toLatin1()
            );


        std::vector<uchar> buffer(
            bytes.begin(),
            bytes.end()
        );


        cv::Mat img =
            cv::imdecode(
                buffer,
                cv::IMREAD_COLOR
            );


        if (img.empty())
            continue;

        if (img.cols > 800)
        {
            cv::resize(
                img,
                img,
                cv::Size(
                    800,
                    img.rows * 800 / img.cols
                )
            );
        }


        cv::Mat feature;

        if (!extractFeature(
                img,
                &feature,
                0))
        {
            int pad =
                std::max(img.cols, img.rows) / 4;

            cv::Mat padded;

            cv::copyMakeBorder(
                img,
                padded,
                pad, pad, pad, pad,
                cv::BORDER_CONSTANT,
                cv::Scalar(0, 0, 0)
            );

            if (!extractFeature(
                    padded,
                    &feature,
                    0))
            {
                continue;
            }
        }


        RegisteredFace registered;


        registered.userId =
            userId;


        registered.feature =
            feature.clone();


        registeredFaces.push_back(
            registered
        );
    }


    if (registeredFaces.empty())
    {
        *err =
            "등록된 얼굴이 없습니다. "
            "앱에서 얼굴 사진으로 회원가입을 먼저 해 주세요.";

        return false;
    }


    return true;
}


// 카메라 이미지의 얼굴을 등록된 얼굴들과 비교하고 위조 여부도 판별
bool recognizeFace(
    const cv::Mat &img,
    int *userId,
    double *score,
    cv::Rect *box,
    FaceState *state,
    std::vector<cv::Rect> *ignored,
    double *real)
{
    cv::Mat feature;
    cv::Rect faceBox;


    if (!extractFeature(
            img,
            &feature,
            &faceBox,
            true,
            state,
            ignored))
    {
        return false;
    }

    if (box)
        *box = faceBox;
    if (real)
        *real = realScore(img, faceBox);


    double bestScore = -1.0;
    int bestUserId = -1;
    for (size_t i = 0;
         i < registeredFaces.size();
         ++i)
    {
        double similarity =
            recognizer->match(
                feature,
                registeredFaces[i].feature,
                cv::FaceRecognizerSF::FR_COSINE
            );


        if (similarity > bestScore)
        {
            bestScore =
                similarity;

            bestUserId =
                registeredFaces[i].userId;
        }
    }


    if (score)
        *score = bestScore;


    if (userId)
        *userId = bestUserId;
    return
        bestUserId >= 0 &&
        bestScore >= MIN_COSINE;
}

// 프레임 한 장의 인식 결과
struct Result
{
    bool known = false;
    int label = -1;
    double score = 0.0;
    double real = 0.0;
    cv::Rect box;
    FaceState state = FACE_NONE;
    std::vector<cv::Rect> ignored;
};

}

// 카메라 + 백그라운드 인식 스레드
struct Face::Cam
{
    cv::VideoCapture cap;
    Result shown;

    void start()
    {
        worker = std::thread(&Cam::run, this);
    }

    void push(const cv::Mat &img)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending = img.clone();
        }
        wake.notify_one();
    }

    bool take(Result *r)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!fresh)
            return false;
        *r = result;
        fresh = false;
        return true;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending.release();
        fresh = false;
        shown = Result();
    }

    ~Cam()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        wake.notify_one();
        if (worker.joinable())
            worker.join();
    }

private:
    void run()
    {
        for (;;) {
            cv::Mat img;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this] { return stop || !pending.empty(); });
                if (stop)
                    return;
                img = pending;
                pending.release();
            }

            Result r;
            try {
                r.known = recognizeFace(img, &r.label, &r.score, &r.box, &r.state, &r.ignored, &r.real);
            } catch (...) {
                r = Result();
            }

            std::lock_guard<std::mutex> lock(mutex);
            result = r;
            fresh = true;
        }
    }

    std::thread worker;
    std::mutex mutex;
    std::condition_variable wake;
    cv::Mat pending;
    Result result;
    bool fresh = false;
    bool stop = false;
};


#else


struct Face::Cam
{
};


#endif


// 얼굴인식 창 초기화: 등록 얼굴을 불러오고 카메라를 열어 33ms 마다 tick 실행
Face::Face(
    const QString &title,
    QWidget *parent,
    bool pw)
    : QDialog(parent),
      ui(new Ui::Face),
      m_cam(0),
      m_timer(new QTimer(this)),
      m_id(-1),
      m_last(-1),
      m_hits(0),
      m_ticks(0),
      m_pw(pw),
      m_byPw(false)
{
    ui->setupUi(this);

    scaleUi(this);

    setWindowTitle(title);

    ui->lblTitle->setText(title);

    ui->btnPw->setVisible(pw);


    connect(
        ui->btnPw,
        SIGNAL(clicked()),
        SLOT(pwLogin())
    );


    connect(
        ui->btnCancel,
        SIGNAL(clicked()),
        SLOT(reject())
    );


#ifdef USE_OPENCV


    connect(
        m_timer,
        SIGNAL(timeout()),
        SLOT(tick())
    );


    QString err;

    if (!train(&err))
    {
        ui->lblStatus->setText(err);
    }
    else
    {
        m_cam = new Cam;
        bool opened = m_cam->cap.open(0, cv::CAP_V4L2);
        if (opened)
        {
            m_cam->cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
            m_cam->cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
            m_cam->cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
            m_cam->cap.set(cv::CAP_PROP_FPS, 30);
            m_cam->cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
        }
        else
        {
            opened = m_cam->cap.open(0);
        }

        if (opened)
        {
            m_cam->start();
            m_timer->start(33);
        }
        else
        {
            ui->lblStatus->setText(
                "카메라를 열 수 없습니다."
            );
        }
    }


#else


    ui->lblStatus->setText(
        "OpenCV 없이 빌드되어 얼굴인식을 사용할 수 없습니다."
    );


#endif
}


// 인식 스레드 종료 후 카메라 닫기
Face::~Face()
{
    delete m_cam;
    delete ui;
}

// 33ms 마다: 카메라 프레임을 읽어 인식 스레드에 넘기고, 결과를 반영해 화면에 그림
void Face::tick()
{
#ifdef USE_OPENCV

    cv::Mat img;

    if (!m_cam->cap.read(img) ||
        img.empty())
    {
        return;
    }
    if (m_ticks > MAX_TICKS)
    {
        m_timer->stop();

        if (m_pw)
        {
            ui->lblStatus->setText(
                "얼굴을 인식하지 못했습니다. "
                "[비밀번호로 로그인]을 눌러 주세요."
            );

            return;
        }

        Msg::warn(
            this,
            windowTitle(),
            "얼굴을 인식하지 못했습니다. 다시 시도해 주세요."
        );

        reject();

        return;
    }

    m_cam->push(img);

    Result &r = m_cam->shown;
    if (m_cam->take(&r))
    {
        ++m_ticks;

        bool fake = r.state == FACE_OK && r.real < REAL_MIN;

        if (fake)
        {
            m_last = -1;
            m_hits = 0;
        }
        else if (r.known &&
            r.label == m_last)
        {
            ++m_hits;
        }
        else if (r.known)
        {
            m_last = r.label;
            m_hits = 1;
        }
        else
        {
            m_last = -1;
            m_hits = 0;
        }

        ui->barHits->setMaximum(HITS);
        ui->barHits->setValue(qMin(m_hits, HITS));

        if (fake)
        {
            ui->lblStatus->setText(
                "사진이나 화면은 사용할 수 없습니다. 실제 얼굴을 비춰 주세요."
            );
        }
        else if (r.known)
        {
            ui->lblStatus->setText(
                QString(
                    "인식 중... (%1/%2)"
                )
                .arg(m_hits)
                .arg(HITS)
            );
        }
        else if (r.state == FACE_OK)
        {
            ui->lblStatus->setText(
                "등록되지 않은 얼굴입니다."
            );
        }
        else if (r.state == FACE_TOO_FAR)
        {
            ui->lblStatus->setText(
                "카메라에 조금 더 가까이 와 주세요."
            );
        }
        else
        {
            ui->lblStatus->setText(
                "흰색 사각형 안에 얼굴을 맞춰 주세요."
            );
        }
    }

    cv::Rect zone = zoneRect(img.size());
    cv::Mat dark = img.clone();
    dark *= 0.4;
    img(zone).copyTo(dark(zone));
    img = dark;
    cv::rectangle(img, zone, cv::Scalar(255, 255, 255), 3);

    for (size_t i = 0; i < r.ignored.size(); ++i)
        cv::rectangle(img, r.ignored[i], cv::Scalar(150, 150, 150), 1);

    if (r.state == FACE_OK &&
        r.box.width > 0 &&
        r.box.height > 0)
    {
        cv::Scalar color =
            r.real < REAL_MIN
                ? cv::Scalar(0, 140, 255)
                : r.known
                    ? cv::Scalar(0, 255, 0)
                    : cv::Scalar(0, 0, 255);

        cv::rectangle(
            img,
            r.box,
            color,
            2
        );
    }

    cv::Mat rgb;
    cv::flip(img,img,1);
    cv::cvtColor(
        img,
        rgb,
        cv::COLOR_BGR2RGB
    );

    QImage qimg(
        rgb.data,
        rgb.cols,
        rgb.rows,
        int(rgb.step),
        QImage::Format_RGB888
    );

    ui->lblCam->setPixmap(
        QPixmap::fromImage(qimg).scaled(
            ui->lblCam->size(),
            Qt::KeepAspectRatio
        )
    );

    if (m_hits >= HITS)
    {
        m_timer->stop();

        m_id = m_last;

        accept();
    }

#endif
}


// [비밀번호로 로그인]: 인식을 멈추고 비밀번호 로그인 창, 취소하면 처음부터 다시 얼굴인식
void Face::pwLogin()
{
    bool running =
        m_timer->isActive();

    m_timer->stop();

    Login dlg(this);
    if (popup(&dlg) == QDialog::Accepted)
    {
        m_id = dlg.id();

        m_byPw = true;

        accept();
    }
    else if (running)
    {
        m_ticks = 0;
        m_last = -1;
        m_hits = 0;
#ifdef USE_OPENCV
        m_cam->clear();
#endif

        m_timer->start(33);
    }
}

