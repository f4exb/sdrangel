#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = demodbfm
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase

CONFIG(ANDROID):INCLUDEPATH += /opt/softs/boost_1_60_0
CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += bfmdemod.cpp\
    bfmdemodgui.cpp\
    bfmplugin.cpp\
    rdsdemod.cpp\
    rdsdecoder.cpp\
    rdsparser.cpp\
    rdstmc.cpp

HEADERS += bfmdemod.h\
    bfmdemodgui.h\
    bfmplugin.h\
    rdsdemod.h\
    rdsdecoder.h\
    rdsparser.h\
    rdstmc.h

FORMS += bfmdemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase

RESOURCES = ../../../sdrbase/resources/res.qrc
