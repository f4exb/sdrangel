#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = chanalyzer
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(ANDROID):INCLUDEPATH += /opt/build/Boost-for-Android-Prebuilt/boost_1_53_0/include
CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += chanalyzer.cpp\
	chanalyzergui.cpp\
	chanalyzerplugin.cpp

HEADERS += chanalyzer.h\
chanalyzergui.h\
chanalyzerplugin.h

FORMS += chanalyzergui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
