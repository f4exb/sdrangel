#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputtestsource

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

SOURCES += testsourcegui.cpp\
	testsourceinput.cpp\
	testsourceplugin.cpp\
	testsourcesettings.cpp\
	testsourcethread.cpp

HEADERS += testsourcegui.h\
	testsourceinput.h\
	testsourceplugin.h\
	testsourcesettings.h\
	testsourcethread.h

FORMS += testsourcegui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger

RESOURCES = ../../../sdrgui/resources/res.qrc
