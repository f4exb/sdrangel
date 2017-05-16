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

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(MINGW64):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(macx):INCLUDEPATH += "../../../../../boost_1_64_0"

SOURCES += atvdemod.cpp\
	atvdemodgui.cpp\
	atvdemodplugin.cpp\
        atvscreen.cpp\
        glshaderarray.cpp

HEADERS += atvdemod.h\
	atvdemodgui.h\
	atvdemodplugin.h\
        atvscreen.h\
        glshaderarray.h

FORMS += atvdemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc