#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = demodbfm

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui

CONFIG(ANDROID):INCLUDEPATH += /opt/softs/boost_1_60_0
CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(MINGW64):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(macx):INCLUDEPATH += "../../../../../boost_1_64_0"

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += bfmdemod.cpp\
    bfmdemodgui.cpp\
    bfmdemodsettings.cpp\
    bfmplugin.cpp\
    rdsdemod.cpp\
    rdsdecoder.cpp\
    rdsparser.cpp\
    rdstmc.cpp

HEADERS += bfmdemod.h\
    bfmdemodgui.h\
    bfmdemodsettings.h\
    bfmplugin.h\
    rdsdemod.h\
    rdsdecoder.h\
    rdsparser.h\
    rdstmc.h

FORMS += bfmdemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui

RESOURCES = ../../../sdrgui/resources/res.qrc
