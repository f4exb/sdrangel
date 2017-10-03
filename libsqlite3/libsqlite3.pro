#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libsqlite3

CONFIG(MINGW32):LIBSQLITE3SRC = "."
CONFIG(MINGW64):LIBSQLITE3SRC = "."

CONFIG(MINGW32):INCLUDEPATH += "src"
CONFIG(MINGW64):INCLUDEPATH += "src"

SOURCES = $$LIBSQLITE3SRC/src/sqlite3.c

HEADERS = $$LIBSQLITE3SRC/src/sqlite3.h\
    $$LIBSQLITE3SRC/src/sqlite3ext.h
    
CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
    
