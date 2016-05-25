#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network opengl

TARGET = inputsdrdaemon

CONFIG(MINGW32):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"
CONFIG(MINGW64):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../lz4
INCLUDEPATH += $$LIBNANOMSGSRC/src

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
LIBS += -L../../../nanomsg/$${build_subdir} -lnanomsg

RESOURCES = ../../../sdrbase/resources/res.qrc

CONFIG(MINGW32):DEFINES += USE_INTERNAL_TIMER=1
