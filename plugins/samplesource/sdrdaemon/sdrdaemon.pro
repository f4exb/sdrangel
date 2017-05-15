#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network opengl

TARGET = inputsdrdaemon

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

CONFIG(MINGW32):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"
CONFIG(MINGW64):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../lz4
!macx:INCLUDEPATH += $$LIBNANOMSGSRC/src
macx:INCLUDEPATH += /opt/local/include

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
!macx:LIBS += -L../../../nanomsg/$${build_subdir} -lnanomsg
macx:LIBS += -L/opt/local/lib -lnanomsg

RESOURCES = ../../../sdrbase/resources/res.qrc

CONFIG(MINGW32):DEFINES += USE_INTERNAL_TIMER=1
