lessThan(QT_MAJOR_VERSION, 5): error("Qt 5 이상이 필요합니다 (JSON 통신 사용)")

QT       += core gui widgets network sql
CONFIG   += c++11
TARGET    = KioskClient
TEMPLATE  = app

INCLUDEPATH += ../common ../server

# 서버는 키오스크 안에서 함께 실행된다 (embeddedserver.cpp)
# server.ini 를 소스 폴더(server/)에서도 찾기 위해 경로를 넣어 둔다 (Qt Creator 빌드 폴더 실행 대응)
DEFINES += SERVER_SOURCE_DIR=\\\"$$clean_path($$PWD/../server)\\\"

HEADERS += ../common/protocol.h \
           qtcompat.h \
           serverclient.h \
           facedialog.h \
           mainwindow.h \
           memberwidget.h \
           cartdialog.h \
           adminwidget.h \
           saleschart.h \
           virtualkeyboard.h \
           passworddialog.h \
           embeddedserver.h \
           kioskdialog.h \
           uiscale.h \
           ../server/database.h \
           ../server/kioskserver.h
SOURCES += main.cpp \
           serverclient.cpp \
           facedialog.cpp \
           mainwindow.cpp \
           memberwidget.cpp \
           cartdialog.cpp \
           adminwidget.cpp \
           saleschart.cpp \
           virtualkeyboard.cpp \
           passworddialog.cpp \
           embeddedserver.cpp \
           kioskdialog.cpp \
           ../server/database.cpp \
           ../server/kioskserver.cpp

DISTFILES += ../../db/kiosk.sql ../server/server.ini.example

# ---------------------------------------------------------------
# 실제 얼굴인식: OpenCV 3.3 이상 + opencv_contrib(face 모듈) 필요
#   qmake "CONFIG+=opencv" 로 빌드. 경로/라이브러리 이름은 환경에 맞게 수정
#   (Windows 빌드는 opencv_core455 처럼 버전 접미사가 붙는다)
# OpenCV 없이 빌드하면 사용자 ID를 직접 입력하는 시뮬레이션 모드로 동작한다.
# ---------------------------------------------------------------
opencv {
    DEFINES += USE_OPENCV
    CONFIG += c++11
    unix {
        # Ubuntu: sudo apt install libopencv-dev libopencv-contrib-dev
        CONFIG += link_pkgconfig
        packagesExist(opencv4): PKGCONFIG += opencv4
        else: PKGCONFIG += opencv
    }
    win32 {
        INCLUDEPATH += C:/opencv/build/include
        LIBS += -LC:/opencv/build/x64/mingw/lib \
                -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
                -lopencv_videoio -lopencv_objdetect -lopencv_face
    }
}
