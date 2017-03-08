#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia

TARGET = modatv

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\opencv\build\include"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\opencv\build\include"

SOURCES += atvmod.cpp\
	atvmodgui.cpp\
	atvmodplugin.cpp

HEADERS += atvmod.h\
	atvmodgui.h\
	atvmodplugin.h

FORMS += atvmodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

CONFIG(MINGW32):LIBS += -LD:\softs\opencv\build\mw32\install\x86\mingw\bin -llibopencv_core2413 -llibopencv_highgui2413
CONFIG(MINGW64):LIBS += -LD:\softs\opencv\build\mw64\install\x64\mingw\bin -llibopencv_core2413 -llibopencv_highgui2413

RESOURCES = ../../../sdrbase/resources/res.qrc
