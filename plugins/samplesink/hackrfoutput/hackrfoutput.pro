#--------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = outputhackrf

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host"
CONFIG(MINGW64):LIBHACKRFSRC = "D:\softs\hackrf\host"
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../devices
!macx:INCLUDEPATH += $$LIBHACKRFSRC
macx:INCLUDEPATH += /opt/local/include

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += hackrfoutputgui.cpp\
    hackrfoutput.cpp\
    hackrfoutputplugin.cpp\
    hackrfoutputsettings.cpp\
    hackrfoutputthread.cpp

HEADERS += hackrfoutputgui.h\
    hackrfoutput.h\
    hackrfoutputplugin.h\
    hackrfoutputsettings.h\
    hackrfoutputthread.h

FORMS += hackrfoutputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
!macx:LIBS += -L../../../libhackrf/$${build_subdir} -llibhackrf
macx:LIBS += -L/opt/local/lib -lhackrf
LIBS += -L../../../devices/$${build_subdir} -ldevices

RESOURCES = ../../../sdrgui/resources/res.qrc
