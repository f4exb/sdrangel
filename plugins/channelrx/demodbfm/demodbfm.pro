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
INCLUDEPATH += ../../../exports
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += ../../../swagger/sdrangel/code/qt5/client

CONFIG(ANDROID):INCLUDEPATH += /opt/softs/boost_1_60_0
CONFIG(MINGW32):INCLUDEPATH += "C:\softs\boost_1_66_0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\boost_1_66_0"
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
LIBS += -L../../../swagger/$${build_subdir} -lswagger

RESOURCES = ../../../sdrgui/resources/res.qrc
