#--------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputbladerf1

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
INCLUDEPATH += ../../../devices

MINGW32 || MINGW64 {
    LIBBLADERF = "C:\Programs\bladeRF"
    INCLUDEPATH += $$LIBBLADERF/include
}

MSVC {
    INCLUDEPATH += "C:\Program Files\PothosSDR\include"
}

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += bladerf1inputgui.cpp\
    bladerf1input.cpp\
    bladerf1inputplugin.cpp\
    bladerf1inputsettings.cpp\
    bladerf1inputthread.cpp

HEADERS += bladerf1inputgui.h\
    bladerf1input.h\
    bladerf1inputplugin.h\
    bladerf1inputsettings.h\
    bladerf1inputthread.h

FORMS += bladerf1inputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -L../../../devices/$${build_subdir} -ldevices

MINGW32 || MINGW64 {
    LIBS += -L$$LIBBLADERF/lib -lbladeRF
}

MSVC {
    LIBS += -L"C:\Program Files\PothosSDR\lib" -L"C:\Program Files\PothosSDR\bin" -lbladeRF
}

RESOURCES = ../../../sdrgui/resources/res.qrc
