lessThan(QT_MAJOR_VERSION, 5): error("Qt 5 이상이 필요합니다 (JSON 통신 사용)")

QT       += core network sql
QT       -= gui
CONFIG   += console c++11
CONFIG   -= app_bundle
TARGET    = KioskServer
TEMPLATE  = app

INCLUDEPATH += ../common

HEADERS += ../common/protocol.h \
           database.h \
           kioskserver.h
SOURCES += main.cpp \
           database.cpp \
           kioskserver.cpp

DISTFILES += schema.sql server.ini.example
