#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libiio

CONFIG(MINGW32):LIBIIOSRC = "D:\softs\libiio"
CONFIG(MINGW64):LIBIIOSRC = "D:\softs\libiio"

INCLUDEPATH += $$PWD/include
INCLUDEPATH += $$LIBIIOSRC

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libusb-1.0.20\include"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\libusb-1.0.20\include"

# LibXml2 Windows distribution from:
# http://xmlsoft.org/sources/win32/
# http://xmlsoft.org/sources/win32/64bit/

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libxml2-2.7.8.win32\include"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\libxml2-2.9.3-win32-x86_64\include\libxml2"

SOURCES = $$LIBIIOSRC/backend.c\
    $$LIBIIOSRC/buffer.c\
    $$LIBIIOSRC/channel.c\
    $$LIBIIOSRC/context.c\
    $$LIBIIOSRC/device.c\
    $$LIBIIOSRC/iiod-client.c\
#    $$LIBIIOSRC/local.c\
    $$LIBIIOSRC/lock.c\
#    $$LIBIIOSRC/network.c\
    $$LIBIIOSRC/scan.c\
#    $$LIBIIOSRC/serial.c\
    $$LIBIIOSRC/usb.c\
    $$LIBIIOSRC/utilities.c\
    $$LIBIIOSRC/xml.c
    
HEADERS = $$LIBIIOSRC/debug.h\
    $$LIBIIOSRC/iio.h\
    $$LIBIIOSRC/iiod-client.h\
    $$LIBIIOSRC/iio-lock.h\
    $$LIBIIOSRC/iio-private.h\
    $$PWD\include/iio-config.h
    
CONFIG(MINGW32):LIBS += -LD:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

CONFIG(MINGW32):LIBS += -LD:\softs\libxml2-2.7.8.win32\bin -llibxml2
CONFIG(MINGW64):LIBS += -LD:\softs\libxml2-2.9.3-win32-x86_64\bin -llibxml2-2

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
