#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = outputfilesink

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += filesinkgui.cpp\
	filesinkoutput.cpp\
	filesinkplugin.cpp\
	filesinksettings.cpp\
	filesinkthread.cpp

HEADERS += filesinkgui.h\
	filesinkoutput.h\
	filesinkplugin.h\
	filesinksettings.h\
	filesinkthread.h

FORMS += filesinkgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger

RESOURCES = ../../../sdrgui/resources/res.qrc
