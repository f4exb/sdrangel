#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = perseus

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

CONFIG(macx):LIBPERSEUSSRC = "../../../../../libperseus-sdr"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += ../../../devices
INCLUDEPATH += $$LIBPERSEUSSRC
macx {
    INCLUDEPATH += /opt/local/include
    INCLUDEPATH += /usr/local/include
}

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += perseusgui.cpp\
    perseusinput.cpp\
    perseusplugin.cpp\
    perseussettings.cpp\
    perseusthread.cpp

HEADERS += perseusgui.h\
    perseusinput.h\
    perseusplugin.h\
    perseussettings.h\
    perseusthread.h

FORMS += perseusgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
LIBS += -L../../../devices/$${build_subdir} -ldevices
macx {
#    LIBS += -L/usr/local/lib -lperseus-sdr
    LIBS += -L../../../libperseus -llibperseus
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = ../../../sdrgui/resources/res.qrc
