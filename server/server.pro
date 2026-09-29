QT       += core network sql
QT       -= gui
CONFIG   += console
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
