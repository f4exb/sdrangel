#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core network

TEMPLATE = lib
TARGET = swagger
INCLUDEPATH += $$PWD/sdrangel/code/qt5/client

win32 {
    DEFINES += __WINDOWS__=1
    DEFINES += DSD_USE_SERIALDV=1
}

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

QMAKE_CXXFLAGS += -std=c++11

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES = $$PWD/sdrangel/code/qt5/client/*.cpp
HEADERS = $$PWD/sdrangel/code/qt5/client/*.h

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
