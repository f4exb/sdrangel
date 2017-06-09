#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network opengl

TARGET = inputsdrdaemonfec

CONFIG(MINGW32):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"
CONFIG(MINGW64):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"

CONFIG(MINGW32):LIBCM256CCSRC = "D:\softs\cm256cc"
CONFIG(MINGW64):LIBCM256CCSRC = "D:\softs\cm256cc"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../lz4
INCLUDEPATH += $$LIBNANOMSGSRC/src
INCLUDEPATH += $$LIBCM256CCSRC

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSSE3=1
QMAKE_CXXFLAGS += -mssse3
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(MINGW64):INCLUDEPATH += "D:\boost_1_58_0"

SOURCES += sdrdaemonfecbuffer.cpp\
sdrdaemonfecgui.cpp\
sdrdaemonfecinput.cpp\
sdrdaemonfecsettings.cpp\
sdrdaemonfecplugin.cpp\
sdrdaemonfecudphandler.cpp

HEADERS += sdrdaemonfecbuffer.h\
sdrdaemonfecgui.h\
sdrdaemonfecinput.h\
sdrdaemonfecsettings.h\
sdrdaemonfecplugin.h\
sdrdaemonfecudphandler.h

FORMS += sdrdaemonfecgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../lz4/$${build_subdir} -llz4
LIBS += -L../../../nanomsg/$${build_subdir} -lnanomsg
LIBS += -L../../../cm256cc/$${build_subdir} -lcm256cc

RESOURCES = ../../../sdrbase/resources/res.qrc

CONFIG(MINGW32):DEFINES += USE_INTERNAL_TIMER=1
