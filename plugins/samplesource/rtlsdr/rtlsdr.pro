#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputrtlsdr

CONFIG(MINGW32):LIBRTLSDRSRC = "D:\softs\librtlsdr"
CONFIG(MINGW64):LIBRTLSDRSRC = "D:\softs\librtlsdr"
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += $$LIBRTLSDRSRC/include

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += rtlsdrgui.cpp\
  rtlsdrinput.cpp\
  rtlsdrplugin.cpp\
  rtlsdrsettings.cpp\
  rtlsdrthread.cpp

HEADERS += rtlsdrgui.h\
  rtlsdrinput.h\
  rtlsdrplugin.h\
  rtlsdrsettings.h\
  rtlsdrthread.h

FORMS += rtlsdrgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../librtlsdr/$${build_subdir} -llibrtlsdr

RESOURCES = ../../../sdrbase/resources/res.qrc
