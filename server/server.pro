lessThan(QT_MAJOR_VERSION, 5): error("Qt 5 이상이 필요합니다 (JSON 통신 사용)")

QT       += core network sql
QT       -= gui
CONFIG   += console c++11
CONFIG   -= app_bundle
TARGET    = KioskServer
TEMPLATE  = app

INCLUDEPATH += ../common

# server.ini 를 소스 폴더에서도 찾기 위해 경로를 넣어 둔다 (Qt Creator 빌드 폴더 실행 대응)
DEFINES += SERVER_SOURCE_DIR=\\\"$$PWD\\\"

HEADERS += ../common/protocol.h \
           database.h \
           kioskserver.h
SOURCES += main.cpp \
           database.cpp \
           kioskserver.cpp

DISTFILES += schema.sql server.ini.example
