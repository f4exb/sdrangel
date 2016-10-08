#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = librtlsdr

CONFIG(MINGW32):LIBRTLSDRSRC = "D:\softs\librtlsdr"
CONFIG(MINGW64):LIBRTLSDRSRC = "D:\softs\librtlsdr"
INCLUDEPATH += $$LIBRTLSDRSRC/include

CONFIG(MINGW32):INCLUDEPATH += "D:\libusb-1.0.19\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "D:\libusb-1.0.19\include\libusb-1.0"

SOURCES = $$LIBRTLSDRSRC/src/librtlsdr.c\
    $$LIBRTLSDRSRC/src/tuner_e4k.c\
    $$LIBRTLSDRSRC/src/tuner_fc0012.c\
    $$LIBRTLSDRSRC/src/tuner_fc0013.c\
    $$LIBRTLSDRSRC/src/tuner_fc2580.c\
    $$LIBRTLSDRSRC/src/tuner_r82xx.c\
    $$LIBRTLSDRSRC/src/getopt/getopt.c

HEADERS = $$LIBRTLSDRSRC/include/rtl-sdr.h\
    $$LIBRTLSDRSRC/include/rtl-sdr_export.h\
    $$LIBRTLSDRSRC/src/getopt/getopt.h

CONFIG(MINGW32):LIBS += -LD:\libusb-1.0.19\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\libusb-1.0.19\MinGW64\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
