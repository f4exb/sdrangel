#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network opengl

TARGET = inputsdrdaemonsource

CONFIG(MINGW32):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"
CONFIG(MINGW64):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"

CONFIG(MINGW32):LIBCM256CCSRC = "D:\softs\cm256cc"
CONFIG(MINGW64):LIBCM256CCSRC = "D:\softs\cm256cc"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += $$LIBNANOMSGSRC/src
INCLUDEPATH += $$LIBCM256CCSRC

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSSE3=1
QMAKE_CXXFLAGS += -mssse3
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(MINGW64):INCLUDEPATH += "D:\boost_1_58_0"

SOURCES += sdrdaemonsourcebuffer.cpp\
sdrdaemonsourcegui.cpp\
sdrdaemonsourceinput.cpp\
sdrdaemonsourcesettings.cpp\
sdrdaemonsourceplugin.cpp\
sdrdaemonsourceudphandler.cpp

HEADERS += sdrdaemonsourcebuffer.h\
sdrdaemonsourcegui.h\
sdrdaemonsourceinput.h\
sdrdaemonsourcesettings.h\
sdrdaemonsourceplugin.h\
sdrdaemonsourceudphandler.h

FORMS += sdrdaemonsourcegui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../nanomsg/$${build_subdir} -lnanomsg
LIBS += -L../../../cm256cc/$${build_subdir} -lcm256cc

RESOURCES = ../../../sdrgui/resources/res.qrc

CONFIG(MINGW32):DEFINES += USE_INTERNAL_TIMER=1
