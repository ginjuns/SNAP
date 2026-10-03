lessThan(QT_MAJOR_VERSION, 5): error("Qt 5 이상이 필요합니다")

QT       += core gui widgets network sql
CONFIG   += c++11
TARGET    = KioskClient
TEMPLATE  = app

INCLUDEPATH += ../common ../server

DEFINES += SERVER_SOURCE_DIR=\\\"$$clean_path($$PWD/../server)\\\"

HEADERS += ../common/packet.h \
           util.h \
           net.h \
           popup.h \
           mainwindow.h \
           member.h \
           cart.h \
           admin.h \
           chart.h \
           calendar.h \
           face.h \
           login.h \
           keyboard.h \
           serverthread.h \
           ../server/db.h \
           ../server/server.h \
           ../server/esp.h

SOURCES += main.cpp \
           net.cpp \
           popup.cpp \
           mainwindow.cpp \
           member.cpp \
           cart.cpp \
           admin.cpp \
           chart.cpp \
           calendar.cpp \
           face.cpp \
           login.cpp \
           keyboard.cpp \
           serverthread.cpp \
           ../server/db.cpp \
           ../server/server.cpp \
           ../server/esp.cpp

FORMS += mainwindow.ui \
         member.ui \
         cart.ui \
         admin.ui \
         calendar.ui \
         face.ui \
         login.ui \
         keyboard.ui

DISTFILES += ../../db/kiosk.sql ../server/server.ini.example

opencv {
    DEFINES += USE_OPENCV
    unix {
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
