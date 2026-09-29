QT       += core gui network
TARGET    = KioskClient
TEMPLATE  = app

INCLUDEPATH += ../common

HEADERS += ../common/protocol.h \
           serverclient.h \
           facedialog.h \
           mainwindow.h \
           memberwidget.h \
           cartdialog.h \
           adminwidget.h
SOURCES += main.cpp \
           serverclient.cpp \
           facedialog.cpp \
           mainwindow.cpp \
           memberwidget.cpp \
           cartdialog.cpp \
           adminwidget.cpp

# ---------------------------------------------------------------
# 실제 얼굴인식: OpenCV 3.3 이상 + opencv_contrib(face 모듈) 필요
#   qmake "CONFIG+=opencv" 로 빌드. 경로/라이브러리 이름은 환경에 맞게 수정
#   (Windows 빌드는 opencv_core455 처럼 버전 접미사가 붙는다)
# OpenCV 없이 빌드하면 사용자 ID를 직접 입력하는 시뮬레이션 모드로 동작한다.
# ---------------------------------------------------------------
opencv {
    DEFINES += USE_OPENCV
    QMAKE_CXXFLAGS += -std=c++11
    INCLUDEPATH += C:/opencv/build/include
    LIBS += -LC:/opencv/build/x64/mingw/lib \
            -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
            -lopencv_videoio -lopencv_objdetect -lopencv_face
}
