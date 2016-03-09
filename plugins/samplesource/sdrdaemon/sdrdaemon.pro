#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network

TARGET = inputsdrdaemon
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../lz4

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += sdrdaemonbuffer.cpp\
sdrdaemongui.cpp\
sdrdaemoninput.cpp\
sdrdaemonplugin.cpp\
sdrdaemonudphandler.cpp

HEADERS += sdrdaemonbuffer.h\
sdrdaemongui.h\
sdrdaemoninput.h\
sdrdaemonplugin.h\
sdrdaemonudphandler.h

FORMS += sdrdaemongui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../lz4/$${build_subdir} -llz4

RESOURCES = ../../../sdrbase/resources/res.qrc
