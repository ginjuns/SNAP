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
const int MAX_TICKS = 400;

/*
 * OpenCV 4.5.4의 FaceDetectorYN은 2023mar 모델을 지원하지 않는다.
 * (detect() 시 "Layer with requested id=-1 not found" 예외)
 * 2023mar 모델은 OpenCV 4.8 이상에서만 사용 가능.
 */
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


/*
 * YuNet + SFace 모델 로드
 */
bool loadModels(QString *err)
{
    /*
     * YuNet
     */
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


    /*
     * SFace
     */
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
    cv::Rect *bestBox)
{
    if (detector.empty() || img.empty())
        return false;


    /*
     * 현재 프레임의 실제 크기를 YuNet에 전달한다.
     *
     * FaceDetectorYN이 내부적으로 필요한 크기로
     * 입력 이미지를 처리한다.
     */
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
     * confidence가 가장 높은 얼굴을 선택한다.
     */
    int bestIndex = 0;

    float bestScore =
        faces.at<float>(0, 14);

    for (int i = 1; i < faces.rows; ++i)
    {
        float score =
            faces.at<float>(i, 14);

        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = i;
        }
    }


    /*
     * FaceRecognizerSF::alignCrop()에서 사용할
     * 1행 15열 detection 데이터를 그대로 복사한다.
     */
    cv::Mat face =
        faces.row(bestIndex).clone();


    if (bestFace)
        *bestFace = face;


    /*
     * 화면에 그릴 얼굴 영역
     */
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


        /*
         * 이미지 범위를 벗어나지 않도록 보정
         */
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


/*
 * 얼굴 이미지 → SFace 특징 벡터
 */
bool extractFeature(
    const cv::Mat &img,
    cv::Mat *feature,
    cv::Rect *box)
{
    if (recognizer.empty())
        return false;


    cv::Mat face;


    /*
     * YuNet으로 얼굴 검출
     */
    if (!detectFace(
            img,
            &face,
            box))
    {
        return false;
    }


    cv::Mat aligned;


    try
    {
        /*
         * YuNet의 5개 landmark를 이용하여
         * SFace 입력 형태로 얼굴 정렬
         */
        recognizer->alignCrop(
            img,
            face,
            aligned
        );


        /*
         * 정렬된 얼굴에서 특징 벡터 추출
         */
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


/*
 * DB에 등록된 얼굴들을 메모리에 학습
 */
bool train(QString *err)
{
    if (!loadModels(err))
        return false;


    QVariant data;


    /*
     * 마지막으로 가져온 얼굴 데이터 이후의
     * 새로운 얼굴 데이터만 가져온다.
     */
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


        /*
         * DB에 Base64로 저장된 이미지 복원
         */
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


        /*
         * 너무 큰 이미지는 축소
         */
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


        /*
         * 얼굴 검출 + 정렬 + 특징 추출
         */
        if (!extractFeature(
                img,
                &feature,
                0))
        {
            /*
             * 얼굴만 잘라 저장된 사진은 YuNet이
             * 검출하지 못할 수 있으므로 여백을 붙여 재시도
             */
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
    cv::Rect *box)
{
    cv::Mat feature;


    /*
     * 현재 얼굴의 SFace 특징 벡터 추출
     */
    if (!extractFeature(
            img,
            &feature,
            box))
    {
        return false;
    }


    double bestScore = -1.0;
    int bestUserId = -1;


    /*
     * 등록된 모든 얼굴과 비교
     */
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


    /*
     * cosine similarity가
     * 기준값 이상이면 인식 성공
     */
    return
        bestUserId >= 0 &&
        bestScore >= MIN_COSINE;
}

}


/*
 * 카메라 구조체
 */
struct Face::Cam
{
    cv::VideoCapture cap;
};


#else


struct Face::Cam
{
};


#endif


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


    /*
     * DB의 얼굴 정보를 불러와
     * SFace 특징 벡터 생성
     */
    if (!train(&err))
    {
        ui->lblStatus->setText(err);
    }
    else
    {
        m_cam = new Cam;


        /*
         * 기본 카메라 열기
         */
        if (m_cam->cap.open(0))
        {
            m_timer->start(50);
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


Face::~Face()
{
    delete m_cam;
    delete ui;
}


/*
 * 카메라 프레임 처리
 */
void Face::tick()
{
#ifdef USE_OPENCV

    cv::Mat img;

    if (!m_cam->cap.read(img) ||
        img.empty())
    {
        return;
    }

    /*
     * 최대 인식 시간 초과
     */
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

    /*
     * 얼굴 인식
     */
    bool known =
        recognizeFace(
            img,
            &label,
            &score,
            &faceBox
        );

    /*
     * 얼굴 검출 영역 표시
     */
    if (faceBox.width > 0 &&
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

    /*
     * 같은 회원이 연속으로 인식되었는지 확인
     */
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

    /*
     * 상태 표시
     */
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
    else
    {
        ui->lblStatus->setText(
            "등록되지 않은 얼굴입니다."
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

    /*
     * 5번 연속 인식되면 로그인 성공
     */
    if (m_hits >= HITS)
    {
        m_timer->stop();

        m_id = m_last;

        accept();
    }

#endif
}


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

        m_timer->start(50);
    }
}


