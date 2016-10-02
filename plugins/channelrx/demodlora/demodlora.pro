#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = demodlora
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += lorademod.cpp\
    lorademodgui.cpp\
    loraplugin.cpp

HEADERS += lorademod.h\
    lorademodgui.h\
    loraplugin.h

FORMS += lorademodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
