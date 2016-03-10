#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network

TARGET = tcpsrc
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += tcpsrc.cpp\
    tcpsrcgui.cpp\
    tcpsrcplugin.cpp

HEADERS += tcpsrc.h\
    tcpsrcgui.h\
    tcpsrcplugin.h

FORMS += tcpsrcgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
