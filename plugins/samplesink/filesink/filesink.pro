#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = outputfilesink
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += filesinkgui.cpp\
	filesinkoutput.cpp\
	filesinkplugin.cpp\
	filesinkthread.cpp

HEADERS += filesinkgui.h\
	filesinkoutput.h\
	filesinkplugin.h\
	filesinkthread.h

FORMS += filesinkgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
