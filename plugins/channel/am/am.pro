#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia

TARGET = demodam
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += amdemod.cpp\
	amdemodgui.cpp\
	amplugin.cpp

HEADERS += amdemod.h\
	amdemodgui.h\
	amplugin.h

FORMS += amdemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc

target.path = ../../../dist/$${build_subdir}/plugins/channel
CONFIG(MINGW32):INSTALLS += target
