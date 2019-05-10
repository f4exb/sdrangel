#--------------------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia network opengl

TARGET = outputlocal

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client
macx:INCLUDEPATH += /opt/local/include

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(macx):INCLUDEPATH += "../../../boost_1_69_0"

SOURCES += localoutputgui.cpp\
localoutput.cpp\
localoutputsettings.cpp\
localoutputplugin.cpp

HEADERS += localoutputgui.h\
localoutput.h\
localoutputsettings.h\
localoutputplugin.h

FORMS += localoutputgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../swagger/$${build_subdir} -lswagger

macx {
    QMAKE_LFLAGS_SONAME = -Wl,-install_name,@rpath/
}

RESOURCES = ../../../sdrgui/resources/res.qrc

CONFIG(MINGW32):DEFINES += USE_INTERNAL_TIMER=1

