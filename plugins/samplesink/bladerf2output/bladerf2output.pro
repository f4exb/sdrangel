#--------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = outputbladerf2

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

SOURCES += bladerf2outputgui.cpp\
    bladerf2output.cpp\
    bladerf2outputplugin.cpp\
    bladerf2outputsettings.cpp\
    bladerf2outputthread.cpp

HEADERS += bladerf2outputgui.h\
    bladerf2output.h\
    bladerf2outputplugin.h\
    bladerf2outputsettings.h\
    bladerf2outputthread.h

FORMS += bladerf2outputgui.ui

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
