#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputplutosdr

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

CONFIG(MINGW32):LIBIIOSRC = "D:\softs\libiio"
CONFIG(MINGW64):LIBIIOSRC = "D:\softs\libiio"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += ../../../devices
INCLUDEPATH += ../../../libiio/includemw
INCLUDEPATH += $$LIBIIOSRC

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += plutosdrinputgui.cpp\
  plutosdrinput.cpp\
  plutosdrinputplugin.cpp\
  plutosdrinputsettings.cpp\
  plutosdrinputthread.cpp

HEADERS += plutosdrinputgui.h\
  plutosdrinput.h\
  plutosdrinputplugin.h\
  plutosdrinputsettings.h\
  plutosdrinputthread.h

FORMS += plutosdrinputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -L../../../libiio/$${build_subdir} -llibiio
LIBS += -L../../../devices/$${build_subdir} -ldevices

RESOURCES = ../../../sdrgui/resources/res.qrc
