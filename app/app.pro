#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core gui multimedia opengl
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TEMPLATE = app
TARGET = sdrangel
INCLUDEPATH += $$PWD/../sdrbase

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += main.cpp
LIBS += -L../sdrbase/$${build_subdir} -lsdrbase

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
