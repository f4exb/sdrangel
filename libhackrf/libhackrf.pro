#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libhackrf

CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host\libhackrf"
CONFIG(MINGW64):LIBHACKRFSRC = "D:\softs\hackrf\host\libhackrf"
INCLUDEPATH += $$LIBHACKRFSRC/src

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"

SOURCES = $$LIBHACKRFSRC/src/hackrf.c

HEADERS = $$LIBHACKRFSRC/src/hackrf.h

CONFIG(MINGW32):LIBS += -LD:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
