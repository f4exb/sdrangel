#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = inputfcdproplus

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
macx:QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
INCLUDEPATH += ../../../fcdhid
INCLUDEPATH += ../../../fcdlib

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES = fcdproplusgui.cpp\
    fcdproplusinput.cpp\
    fcdproplusplugin.cpp\
    fcdproplussettings.cpp\
    fcdproplusthread.cpp

HEADERS = fcdproplusgui.h\
    fcdproplusinput.h\
    fcdproplusplugin.h\
    fcdproplussettings.h\
    fcdproplusthread.h

FORMS += fcdproplusgui.ui

LIBS += -L../../../fcdlib/$${build_subdir} -lfcdlib
LIBS += -L../../../fcdhid/$${build_subdir} -lfcdhid
LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger
