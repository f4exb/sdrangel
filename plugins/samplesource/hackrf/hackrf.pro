#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputhackrf

CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host"
CONFIG(MINGW64):LIBHACKRFSRC = "D:\softs\hackrf\host"
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += $$LIBHACKRFSRC

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += hackrfgui.cpp\
  hackrfinput.cpp\
  hackrfplugin.cpp\
  hackrfsettings.cpp\
  hackrfthread.cpp

HEADERS += hackrfgui.h\
  hackrfinput.h\
  hackrfplugin.h\
  hackrfsettings.h\
  hackrfthread.h

FORMS += hackrfgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../libhackrf/$${build_subdir} -llibhackrf

RESOURCES = ../../../sdrbase/resources/res.qrc
