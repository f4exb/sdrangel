#--------------------------------------------
#
# Pro file for Windows build with Qt Creator
#
#--------------------------------------------

QT += core

TEMPLATE = lib
TARGET = fcdhid

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.20\include"
CONFIG(MINGW32):DEFINES += MINGW32=1

CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.20\include"
CONFIG(MINGW64):DEFINES += MINGW32=1

CONFIG(macx):INCLUDEPATH += "/opt/local/include"

SOURCES = $$PWD/hid-libusb.c\
    $$PWD/fcdhid.c

macx:SOURCES += ../apple/apple_compat.c

HEADERS = $$PWD/fcdhid.h\
    $$PWD/hid-libusb.h\
    $$PWD/hidapi.h

CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0 -liconv
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0 -liconv
CONFIG(macx):LIBS += -L/opt/local/lib -lusb-1.0 -liconv
