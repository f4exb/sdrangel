#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = lz4
INCLUDEPATH += $$PWD

SOURCES = lz4.c\
        xxhash.c

HEADERS = lz4.h\
        xxhash.h

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
