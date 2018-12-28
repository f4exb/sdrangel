#--------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputbladerf2

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

MINGW32 {
    LIBBLADERF = "C:\Programs\bladeRF"
    INCLUDEPATH += $$LIBBLADERF/include
}

MSVC {
    INCLUDEPATH += "C:\Program Files\PothosSDR\include"
}

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += bladerf2inputgui.cpp\
    bladerf2input.cpp\
    bladerf2inputplugin.cpp\
    bladerf2inputsettings.cpp\
    bladerf2inputthread.cpp

HEADERS += bladerf2inputgui.h\
    bladerf2input.h\
    bladerf2inputplugin.h\
    bladerf2inputsettings.h\
    bladerf2inputthread.h

FORMS += bladerf2inputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -L../../../devices/$${build_subdir} -ldevices

MINGW32 {
    LIBS += -L$$LIBBLADERF/lib -lbladeRF
}

MSVC {
    LIBS += -L"C:\Program Files\PothosSDR\lib" -L"C:\Program Files\PothosSDR\bin" -lbladeRF
}

RESOURCES = ../../../sdrgui/resources/res.qrc
