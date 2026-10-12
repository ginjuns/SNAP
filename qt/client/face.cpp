#include "face.h"
#include "ui_face.h"
#include "login.h"
#include "net.h"
#include "popup.h"
#include "util.h"

#ifdef USE_OPENCV

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect/face.hpp>

namespace {

const double MIN_COSINE = 0.363;
const int HITS = 5;
const int MAX_TICKS = 600;

/*
 * 인식 구역
 *
 * 뒤에 서 있는 사람이 인식되지 않도록 카메라 화면 가운데의
 * 구역 안에 얼굴 중심이 있고, 충분히 가까이 있는(크게 보이는)
 * 얼굴만 인식한다. 값은 화면 크기에 대한 비율.
 *
 * - ZONE_W, ZONE_H : 인식 구역의 가로/세로 (화면 가운데 기준)
 * - MIN_FACE_W     : 얼굴 너비가 화면 너비의 이 비율보다 작으면
 *                    멀리 있는 사람으로 보고 무시
 *                    (뒤에 사람이 계속 인식되면 값을 키운다)
 */
const double ZONE_W = 0.5;
const double ZONE_H = 0.8;
const double MIN_FACE_W = 0.15;

enum FaceState
{
    FACE_NONE,      // 얼굴 없음
    FACE_OUTSIDE,   // 구역 밖의 얼굴만 있음
    FACE_TOO_FAR,   // 구역 안이지만 너무 멀리 있음
    FACE_OK
};

// 카메라 화면 가운데의 인식 구역 사각형
cv::Rect zoneRect(const cv::Size &size)
{
    int w = cvRound(size.width * ZONE_W);
    int h = cvRound(size.height * ZONE_H);

    return cv::Rect((size.width - w) / 2, (size.height - h) / 2, w, h);
}

const char *YUNET_MODEL =
    "/home/jun/Smart_Face_Kiosk/qt/client/models/face_detection_yunet_2022mar.onnx";

const char *SFACE_MODEL =
    "/home/jun/Smart_Face_Kiosk/qt/client/models/face_recognition_sface_2021dec.onnx";

cv::Ptr<cv::FaceDetectorYN> detector;
cv::Ptr<cv::FaceRecognizerSF> recognizer;

struct RegisteredFace
{
    int userId;
    cv::Mat feature;
};

std::vector<RegisteredFace> registeredFaces;

int lastId = 0;



// YuNet + SFace 모델 로드
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

    return true;
}


/*
 * YuNet으로 얼굴 검출
 *
 * FaceDetectorYN::detect() 결과는 한 얼굴당
 *
 * [x, y, w, h,
 *  right_eye_x, right_eye_y,
 *  left_eye_x, left_eye_y,
 *  nose_x, nose_y,
 *  right_mouth_x, right_mouth_y,
 *  left_mouth_x, left_mouth_y,
 *  score]
 *
 * 총 15개의 값으로 구성된다.
 */
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


    /*
     * 여러 얼굴이 발견되면
     * - 카메라 화면(useZone): 인식 구역 안의 충분히 큰 얼굴 중 가장 큰 얼굴
     *   (키오스크 바로 앞에 선 사람)
     * - 등록 사진: confidence가 가장 높은 얼굴
     */
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

// 얼굴 이미지 → SFace 특징 벡터
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

// DB에 등록된 얼굴들을 메모리에 학습
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


/*
 * 현재 카메라 이미지의 얼굴을
 * 등록된 얼굴들과 비교
 */
bool recognizeFace(
    const cv::Mat &img,
    int *userId,
    double *score,
    cv::Rect *box,
    FaceState *state,
    std::vector<cv::Rect> *ignored)
{
    cv::Mat feature;


    /*
     * 현재 얼굴의 SFace 특징 벡터 추출
     */
    if (!extractFeature(
            img,
            &feature,
            box,
            true,
            state,
            ignored))
    {
        return false;
    }


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

}

// 카메라 구조체
struct Face::Cam
{
    cv::VideoCapture cap;
};


#else


struct Face::Cam
{
};


#endif


// 얼굴인식 창 초기화: 등록 얼굴을 불러오고 카메라를 열어 50ms 마다 tick 실행
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

        /*
         * 카메라를 640x480 / MJPG / 30fps 로 연다.
         * 그냥 open(0) 하면 카메라의 최대 해상도(C920: 2560x1472)를 압축 없이 받아
         * 초당 2~5프레임밖에 안 나와 화면이 끊긴다.
         * V4L2 로 못 열면 예전 방식(open(0))으로 연다.
         */
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


// 카메라 닫기
Face::~Face()
{
    delete m_cam;
    delete ui;
}

// 카메라 프레임 처리
void Face::tick()
{
#ifdef USE_OPENCV

    cv::Mat img;

    if (!m_cam->cap.read(img) ||
        img.empty())
    {
        return;
    }
    if (++m_ticks > MAX_TICKS)
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

    cv::Rect faceBox;

    int label = -1;
    double score = 0.0;
    FaceState state = FACE_NONE;
    std::vector<cv::Rect> ignored;
    bool known =
        recognizeFace(
            img,
            &label,
            &score,
            &faceBox,
            &state,
            &ignored
        );
    cv::Rect zone = zoneRect(img.size());
    cv::Mat dark = img.clone();
    dark *= 0.4;
    img(zone).copyTo(dark(zone));
    img = dark;
    cv::rectangle(img, zone, cv::Scalar(255, 255, 255), 3);

    for (size_t i = 0; i < ignored.size(); ++i)
        cv::rectangle(img, ignored[i], cv::Scalar(150, 150, 150), 1);

    if (state == FACE_OK &&
        faceBox.width > 0 &&
        faceBox.height > 0)
    {
        cv::Scalar color =
            known
                ? cv::Scalar(0, 255, 0)
                : cv::Scalar(0, 0, 255);

        cv::rectangle(
            img,
            faceBox,
            color,
            2
        );
    }

    if (known &&
        label == m_last)
    {
        ++m_hits;
    }
    else if (known)
    {
        m_last = label;
        m_hits = 1;
    }
    else
    {
        m_last = -1;
        m_hits = 0;
    }

    // 오른쪽 진행 막대: 연속 인식 횟수 (HITS 번이면 성공)
    ui->barHits->setMaximum(HITS);
    ui->barHits->setValue(qMin(m_hits, HITS));

    if (known)
    {
        ui->lblStatus->setText(
            QString(
                "인식 중... (%1/%2)"
            )
            .arg(m_hits)
            .arg(HITS)
        );
    }
    else if (state == FACE_OK)
    {
        ui->lblStatus->setText(
            "등록되지 않은 얼굴입니다."
        );
    }
    else if (state == FACE_TOO_FAR)
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

    /*
     * BGR → RGB
     */
    cv::Mat rgb;

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


// [비밀번호로 로그인]: 카메라를 멈추고 비밀번호 로그인 창, 취소하면 다시 얼굴인식
void Face::pwLogin()
{
    bool running =
        m_timer->isActive();

    m_timer->stop();

    Login dlg(this);

    if (popup(&dlg) ==
        QDialog::Accepted)
    {
        m_id = dlg.id();

        m_byPw = true;

        accept();
    }
    else if (running)
    {
        m_ticks = 0;

        m_timer->start(33);
    }
}


