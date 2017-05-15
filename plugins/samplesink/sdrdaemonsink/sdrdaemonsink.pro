#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network opengl

TARGET = outputsdrdaemonsink

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSSE3=1
QMAKE_CXXFLAGS += -mssse3
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

CONFIG(MINGW32):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"
CONFIG(MINGW64):LIBNANOMSGSRC = "D:\softs\nanomsg-0.8-beta"
CONFIG(MINGW32):LIBCM256CCSRC = "D:\softs\cm256cc"
CONFIG(MINGW64):LIBCM256CCSRC = "D:\softs\cm256cc"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += $$LIBNANOMSGSRC/src
INCLUDEPATH += $$LIBCM256CCSRC
CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(MINGW64):INCLUDEPATH += "D:\boost_1_58_0"

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += sdrdaemonsinksettings.cpp
#   sdrdaemonsinkgui.cpp\
#	sdrdaemonsinkoutput.cpp\
#	sdrdaemonsinkplugin.cpp\
#	sdrdaemonsinksettings.cpp\
#	sdrdaemonsinkthread.cpp

HEADERS += sdrdaemonsinksettings.h
#   sdrdaemonsinkgui.h\
#	sdrdaemonsinkoutput.h\
#	sdrdaemonsinkplugin.h\
#	sdrdaemonsinksettings.h\
#	sdrdaemonsinkthread.h

FORMS += sdrdaemonsinkgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../devices/$${build_subdir} -ldevices
LIBS += -L../../../nanomsg/$${build_subdir} -lnanomsg
LIBS += -L../../../cm256cc/$${build_subdir} -lcm256cc

RESOURCES = ../../../sdrbase/resources/res.qrc
