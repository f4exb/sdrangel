#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia

TARGET = modam
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

DEFINES += USE_SIMD=1

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += ammod.cpp\
	ammodgui.cpp\
	ammodplugin.cpp

HEADERS += ammod.h\
	ammodgui.h\
	ammodplugin.h

FORMS += ammodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
