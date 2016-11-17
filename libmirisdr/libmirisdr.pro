#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libmirisdr

CONFIG(MINGW32):LIBMIRISDRSRC = "D:\softs\libmirisdr-4"
CONFIG(MINGW64):LIBMIRISDRSRC = "D:\softs\libmirisdr-4"
INCLUDEPATH += $$LIBMIRISDRSRC/include
INCLUDEPATH += $$LIBMIRISDRSRC/src
INCLUDEPATH += $$LIBMIRISDRSRC/src/convert

CONFIG(MINGW32):INCLUDEPATH += "D:\libusb-1.0.19\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "D:\libusb-1.0.19\include\libusb-1.0"

SOURCES = $$LIBMIRISDRSRC/src/libmirisdr.c \
    $$LIBMIRISDRSRC/src/getopt/getopt.c

HEADERS = $$LIBMIRISDRSRC/include/mirisdr.h\
    $$LIBMIRISDRSRC/include/mirisdr_export.h\
    $$LIBMIRISDRSRC/src/getopt/getopt.h\
    $$LIBMIRISDRSRC/src/convert/252_s16.c\
    $$LIBMIRISDRSRC/src/convert/336_s16.c\
    $$LIBMIRISDRSRC/src/convert/384_s16.c\
    $$LIBMIRISDRSRC/src/convert/504_s16.c\
    $$LIBMIRISDRSRC/src/convert/504_s8.c\
    $$LIBMIRISDRSRC/src/convert/base.c\
    $$LIBMIRISDRSRC/src/adc.c\
    $$LIBMIRISDRSRC/src/async.c\
    $$LIBMIRISDRSRC/src/constants.h\
    $$LIBMIRISDRSRC/src/convenience.c\
    $$LIBMIRISDRSRC/src/devices.c\
    $$LIBMIRISDRSRC/src/gain.c\
    $$LIBMIRISDRSRC/src/gain.h\
    $$LIBMIRISDRSRC/src/hard.c\
    $$LIBMIRISDRSRC/src/hard.h\
    $$LIBMIRISDRSRC/src/libusb.h\
    $$LIBMIRISDRSRC/src/reg.c\
    $$LIBMIRISDRSRC/src/soft.c\
    $$LIBMIRISDRSRC/src/soft.h\
    $$LIBMIRISDRSRC/src/streaming.c\
    $$LIBMIRISDRSRC/src/structs.h\
    $$LIBMIRISDRSRC/src/sync.c

CONFIG(MINGW32):LIBS += -LD:\libusb-1.0.19\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\libusb-1.0.19\MinGW64\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
