#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network

TARGET = inputfcdproplus

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../fcdhid
INCLUDEPATH += ../../../fcdlib

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES = fcdproplusgui.cpp\
	fcdproplusinputqt.cpp\
	fcdproplusplugin.cpp\
	fcdproplussettings.cpp\
	fcdproplusreader.cpp

HEADERS = fcdproplusgui.h\
	fcdproplusinputqt.h\
	fcdproplusplugin.h\
	fcdproplussettings.h\
	fcdproplusreader.h

FORMS += fcdproplusgui.ui

LIBS += -L../../../fcdlib/$${build_subdir} -lfcdlib
LIBS += -L../../../fcdhid/$${build_subdir} -lfcdhid
LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
