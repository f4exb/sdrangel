#--------------------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = cm256cc

CONFIG(MINGW32):LIBCM256CCSRC = "C:\softs\cm256cc"
CONFIG(MINGW64):LIBCM256CCSRC = "C:\softs\cm256cc"
CONFIG(macx):LIBCM256CCSRC = "../../deps/cm256cc"

INCLUDEPATH += $$LIBCM256CCSRC

!macx:DEFINES += __WINDOWS__=1
DEFINES += USE_SSSE3=1
QMAKE_CXXFLAGS += -mssse3

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES = $$LIBCM256CCSRC/gf256.cpp\
$$LIBCM256CCSRC/cm256.cpp

HEADERS = $$LIBCM256CCSRC/gf256.h\
$$LIBCM256CCSRC/cm256.h
