#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libairspyhf

CONFIG(MINGW32):LIBAIRSPYHFSRC = "D:\softs\libairspyhf\libairspyhf"
CONFIG(MINGW64):LIBAIRSPYHFSRC = "D:\softs\libairspyhf\libairspyhf"
INCLUDEPATH += $$LIBAIRSPYHFSRC/src

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"

SOURCES = $$LIBAIRSPYSRC/src/airspyhf.c\
  $$LIBAIRSPYSRC/src/iqbalancer.c

HEADERS = $$LIBAIRSPYSRC/src/airspyhf.h\
  $$LIBAIRSPYSRC/src/airspyhf_commands.h\
  $$LIBAIRSPYSRC/src/iqbalancer.h

CONFIG(MINGW32):LIBS += -LD:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
