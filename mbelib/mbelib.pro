#--------------------------------------------------------
#
# Pro file for Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = mbelib

CONFIG(MINGW32):LIBMBELIBSRC = "C:\softs\mbelib"
CONFIG(MINGW64):LIBMBELIBSRC = "C:\softs\mbelib"
CONFIG(macx):LIBMBELIBSRC = "../../deps/mbelib"

INCLUDEPATH += $$LIBMBELIBSRC

SOURCES = $$LIBMBELIBSRC/ambe3600x2400.c\
$$LIBMBELIBSRC/ambe3600x2450.c\
$$LIBMBELIBSRC/ecc.c\
$$LIBMBELIBSRC/imbe7100x4400.c\
$$LIBMBELIBSRC/imbe7200x4400.c\
$$LIBMBELIBSRC/mbelib.c

HEADERS = $$LIBMBELIBSRC/ambe3600x2400_const.h\
$$LIBMBELIBSRC/ambe3600x2450_const.h\
$$LIBMBELIBSRC/ecc_const.h\
$$LIBMBELIBSRC/imbe7200x4400_const.h\
$$LIBMBELIBSRC/mbelib.h\
$$LIBMBELIBSRC/mbelib_const.h
