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

CONFIG(MINGW32):LIBHACKRFSRC = "C:\softs\hackrf\host"
CONFIG(macx):LIBHACKRFSRC = "/opt/local/include"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += ../../../devices
CONFIG(!MSVC):INCLUDEPATH += $$LIBHACKRFSRC
CONFIG(MSVC):INCLUDEPATH += "C:\Program Files\PothosSDR\include"

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
LIBS += -L../../../swagger/$${build_subdir} -lswagger
CONFIG(!MSVC):LIBS += -L../../../libhackrf/$${build_subdir} -llibhackrf
CONFIG(MSVC):LIBS += -L"C:\Program Files\PothosSDR\bin" -L"C:\Program Files\PothosSDR\lib" -lhackrf
LIBS += -L../../../devices/$${build_subdir} -ldevices
macx {
    LIBS -= -L../../../libhackrf/$${build_subdir} -llibhackrf
    LIBS += -L/opt/local/lib -lhackrf
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = ../../../sdrgui/resources/res.qrc
