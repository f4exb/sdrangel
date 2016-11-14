#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputrtlsdr

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../libsdrplay

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += sdrplaygui.cpp\
  sdrplayinput.cpp\
  sdrplayplugin.cpp\
  sdrplaysettings.cpp\
  sdrplaythread.cpp

HEADERS += sdrplaygui.h\
  sdrplayinput.h\
  sdrplayplugin.h\
  sdrplaysettings.h\
  sdrplaythread.h

FORMS += sdrplaygui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
