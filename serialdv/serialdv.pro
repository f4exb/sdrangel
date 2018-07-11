#--------------------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = serialdv

CONFIG(MINGW32):LIBSERIALDVSRC = "C:\softs\serialDV"
CONFIG(MINGW64):LIBSERIALDVSRC = "C:\softs\serialDV"

INCLUDEPATH += $$LIBSERIALDVSRC

DEFINES += __WINDOWS__=1

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES = $$LIBSERIALDVSRC/dvcontroller.cpp\
$$LIBSERIALDVSRC/serialdatacontroller.cpp

HEADERS = $$LIBSERIALDVSRC/dvcontroller.h\
$$LIBSERIALDVSRC/serialdatacontroller.h
