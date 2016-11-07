#--------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputbladerf

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

CONFIG(MINGW32):LIBBLADERFSRC = "D:\softs\bladeRF\host\libraries\libbladeRF\include"
CONFIG(MINGW64):LIBBLADERFSRC = "D:\softs\bladeRF\host\libraries\libbladeRF\include"
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += $$LIBBLADERFSRC

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += bladerfgui.cpp\
    bladerfinput.cpp\
    bladerfplugin.cpp\
    bladerfsettings.cpp\
    bladerfthread.cpp

HEADERS += bladerfgui.h\
    bladerfinput.h\
    bladerfplugin.h\
    bladerfsettings.h\
    bladerfthread.h

FORMS += bladerfgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../libbladerf/$${build_subdir} -llibbladerf

RESOURCES = ../../../sdrbase/resources/res.qrc
