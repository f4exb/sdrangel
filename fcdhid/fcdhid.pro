#--------------------------------------------
#
# Pro file for Windows build with Qt Creator
#
#--------------------------------------------

QT += core

TEMPLATE = lib
TARGET = fcdhid

CONFIG(MINGW32):INCLUDEPATH += "D:\libusb-1.0.19\include"
CONFIG(MINGW32):DEFINES += MINGW32=1

SOURCES = $$PWD/hid-libusb.c\
    $$PWD/fcdhid.c

HEADERS = $$PWD/fcdhid.h\
    $$PWD/hid-libusb.h\
    $$PWD/hidapi.h

CONFIG(MINGW32):LIBS += -LD:\libusb-1.0.19\MinGW32\dll -llibusb-1.0 -liconv
