#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = demodatv

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(macx):INCLUDEPATH += "../../../../../boost_1_64_0"

SOURCES += atvdemod.cpp\
	atvdemodgui.cpp\
	atvdemodplugin.cpp

HEADERS += atvdemod.h\
	atvdemodgui.h\
	atvdemodplugin.h

FORMS += atvdemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui

RESOURCES = ../../../sdrgui/resources/res.qrc
