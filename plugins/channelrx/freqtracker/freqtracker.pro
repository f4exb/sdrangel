#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia

TARGET = freqtracker

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

SOURCES += freqtracker.cpp\
	freqtrackergui.cpp\
	freqtrackerplugin.cpp\
    freqtrackersettings.cpp

HEADERS += freqtracker.h\
	freqtrackergui.h\
	freqtrackerplugin.h\
    freqtrackersettings.h

FORMS += freqtrackergui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger

macx {
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = ../../../sdrgui/resources/res.qrc
