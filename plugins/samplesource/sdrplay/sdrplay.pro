#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputsdrplay

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

CONFIG(MINGW32):INCLUDEPATH += "D:\libusb-1.0.19\include"
CONFIG(MINGW64):INCLUDEPATH += "D:\libusb-1.0.19\include"
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += $$LIBMIRISDRSRC/include

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
LIBS += -L../../../libmirisdr/$${build_subdir} -llibmirisdr

RESOURCES = ../../../sdrbase/resources/res.qrc
