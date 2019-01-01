#--------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = outputlimesdr

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

CONFIG(MINGW32):LIBLIMESUITESRC = "C:\softs\LimeSuite"
CONFIG(macx):LIBLIMESUITESRC = "../../../../../LimeSuite-18.10.0"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += ../../../devices

MINGW32 || macx {
    INCLUDEPATH += ../../../liblimesuite/srcmw
    INCLUDEPATH += $$LIBLIMESUITESRC/src
    INCLUDEPATH += $$LIBLIMESUITESRC/src/ADF4002
    INCLUDEPATH += $$LIBLIMESUITESRC/src/ConnectionRegistry
    INCLUDEPATH += $$LIBLIMESUITESRC/src/FPGA_common
    INCLUDEPATH += $$LIBLIMESUITESRC/src/GFIR
    INCLUDEPATH += $$LIBLIMESUITESRC/src/lms7002m
    INCLUDEPATH += $$LIBLIMESUITESRC/src/lms7002m_mcu
    INCLUDEPATH += $$LIBLIMESUITESRC/src/Si5351C
    INCLUDEPATH += $$LIBLIMESUITESRC/src/protocols
    INCLUDEPATH += $$LIBLIMESUITESRC/external/cpp-feather-ini-parser
}

MSVC {
    INCLUDEPATH += "C:\Program Files\PothosSDR\include"
}

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += limesdroutputgui.cpp\
    limesdroutput.cpp\
    limesdroutputplugin.cpp\
    limesdroutputsettings.cpp\
    limesdroutputthread.cpp

HEADERS += limesdroutputgui.h\
    limesdroutput.h\
    limesdroutputplugin.h\
    limesdroutputsettings.h\
    limesdroutputthread.h

FORMS += limesdroutputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -L../../../devices/$${build_subdir} -ldevices

MINGW32 {
    LIBS += -L../../../liblimesuite/$${build_subdir} -lliblimesuite
}
MSVC {
    LIBS += -L"C:\Program Files\PothosSDR\bin" -L"C:\Program Files\PothosSDR\lib" -lLimeSuite
}
macx {
    LIBS += -L/opt/install/LimeSuite/lib/ -lLimeSuite
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = ../../../sdrgui/resources/res.qrc
