#--------------------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------------------

TEMPLATE = lib
CONFIG += plugin

QT += core gui widgets multimedia opengl

TARGET = demoddsd

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1
QMAKE_CXXFLAGS += -std=c++11

CONFIG(MINGW32):LIBDSDCCSRC = "D:\softs\dsdcc"
CONFIG(MINGW64):LIBDSDCCSRC = "D:\softs\dsdcc"
CONFIG(macx):LIBDSDCCSRC = "../../../../deps/dsdcc"

CONFIG(MINGW32):LIBMBELIBSRC = "D:\softs\mbelib"
CONFIG(MINGW64):LIBMBELIBSRC = "D:\softs\mbelib"
CONFIG(macx):LIBMBELIBSRC = "../../../../deps/mbelib"

CONFIG(MINGW32):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(MINGW64):INCLUDEPATH += "D:\boost_1_58_0"
CONFIG(macx):INCLUDEPATH += "../../../../../boost_1_64_0"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += ../../../sdrgui
INCLUDEPATH += $$LIBDSDCCSRC
INCLUDEPATH += $$LIBMBELIBSRC

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES = dsddecoder.cpp\
dsddemod.cpp\
dsddemodgui.cpp\
dsddemodplugin.cpp\
dsddemodbaudrates.cpp\
dsddemodsettings.cpp

HEADERS = dsddecoder.h\
dsddemod.h\
dsddemodgui.h\
dsddemodplugin.h\
dsddemodbaudrates.h\
dsddemodsettings.h

FORMS = dsddemodgui.ui

LIBS += -L../../../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../../../sdrgui/$${build_subdir} -lsdrgui
LIBS += -L../../../dsdcc/$${build_subdir} -ldsdcc

RESOURCES = ../../../sdrgui/resources/res.qrc
