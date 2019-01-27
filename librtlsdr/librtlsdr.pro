#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = librtlsdr

CONFIG(MSVC):DEFINES += rtlsdr_EXPORTS

CONFIG(MINGW32):LIBRTLSDRSRC = "C:\softs\librtlsdr"
CONFIG(MINGW64):LIBRTLSDRSRC = "C:\softs\librtlsdr"
CONFIG(MSVC):LIBRTLSDRSRC = "C:\softs\librtlsdr"

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.21\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.21\include\libusb-1.0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\libusb-1.0.21\include\libusb-1.0"
CONFIG(macx):INCLUDEPATH += /opt/local/include
CONFIG(MSVC):INCLUDEPATH += "C:\softs\librtlsdr\include"

SOURCES = $$LIBRTLSDRSRC/src/librtlsdr.c\
    $$LIBRTLSDRSRC/src/tuner_e4k.c\
    $$LIBRTLSDRSRC/src/tuner_fc0012.c\
    $$LIBRTLSDRSRC/src/tuner_fc0013.c\
    $$LIBRTLSDRSRC/src/tuner_fc2580.c\
    $$LIBRTLSDRSRC/src/tuner_r82xx.c\
    $$LIBRTLSDRSRC/src/getopt/getopt.c\
    $$LIBRTLSDRSRC/src/convenience/convenience.c

HEADERS = $$LIBRTLSDRSRC/include/reg_field.h\
    $$LIBRTLSDRSRC/include/rtl-sdr_export.h\
    $$LIBRTLSDRSRC/include/rtlsdr_i2c.h\
    $$LIBRTLSDRSRC/include/rtl-sdr.h\
    $$LIBRTLSDRSRC/include/tuner_e4k.h\
    $$LIBRTLSDRSRC/include/tuner_fc0012.h\
    $$LIBRTLSDRSRC/include/tuner_fc0013.h\  
    $$LIBRTLSDRSRC/include/tuner_fc2580.h\
    $$LIBRTLSDRSRC/include/tuner_r82xx.h\
    $$LIBRTLSDRSRC/src/getopt/getopt.h\
    $$LIBRTLSDRSRC/src/convenience/convenience.h

CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.21\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.21\MinGW64\dll -llibusb-1.0
CONFIG(MSVC):LIBS += -LC:\softs\libusb-1.0.21\MS64\dll -llibusb-1.0
macx {
    SOURCES =
    HEADERS =
    LIBS += -L/opt/local/lib -lrtlsdr
}

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
