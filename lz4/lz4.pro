#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = lz4
INCLUDEPATH += $$PWD

SOURCES = lz4core.c\
        lz4frame.c\
        lz4hc.c\
        xxhash.c

HEADERS = lz4.h\
        lz4frame_static.h\
        lz4frame.h\
        lz4hc.h\
        xxhash.h

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
