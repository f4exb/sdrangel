#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia

TARGET = demodam

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += amdemod.cpp\
	amdemodgui.cpp\
	amdemodplugin.cpp\
        amdemodsettings.cpp\
        amdemodssbdialog.cpp

HEADERS += amdemod.h\
	amdemodgui.h\
	amdemodplugin.h\
        amdemodsettings.h\
        amdemodssbdialog.h

FORMS += amdemodgui.ui\
      amdemodssb.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger

RESOURCES = ../../../sdrgui/resources/res.qrc
