#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libhackrf

CONFIG(MINGW32):LIBHACKRFSRC = "C:\softs\hackrf\host\libhackrf"
CONFIG(MINGW64):LIBHACKRFSRC = "C:\softs\hackrf\host\libhackrf"
CONFIG(MSVC):LIBHACKRFSRC = "C:\softs\hackrf\host\libhackrf"

INCLUDEPATH += $$LIBHACKRFSRC/src

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.22\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.22\include\libusb-1.0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\libusb-1.0.22\include\libusb-1.0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\pthreads-w32\include"
CONFIG(macx):INCLUDEPATH += "/opt/local/include"

SOURCES = $$LIBHACKRFSRC/src/hackrf.c

HEADERS = $$LIBHACKRFSRC/src/hackrf.h

CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.22\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.22\MinGW64\dll -llibusb-1.0
CONFIG(MSVC):LIBS += -LC:\softs\libusb-1.0.22\MS64\dll -llibusb-1.0
CONFIG(MSVC):LIBS += -LC:\softs\pthreads-w32\lib\x64 -lpthreadVC2
macx {
    SOURCES =
    HEADERS =
    LIBS += -L/opt/local/lib -lhackrf
}

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
