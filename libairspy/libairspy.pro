#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libairspy

CONFIG(MINGW32):LIBAIRSPYSRC = "D:\softs\libairspy\libairspy"
INCLUDEPATH += $$LIBAIRSPYSRC/src

CONFIG(MINGW32):INCLUDEPATH += "D:\libusb-1.0.19\include\libusb-1.0"

SOURCES = $$LIBAIRSPYSRC/src/airspy.c\
  $$LIBAIRSPYSRC/src/iqconverter_float.c\
  $$LIBAIRSPYSRC/src/iqconverter_int16.c

HEADERS = $$LIBAIRSPYSRC/src/airspy.h\
  $$LIBAIRSPYSRC/src/airspy_commands.h\
  $$LIBAIRSPYSRC/src/iqconverter_float.h\
  $$LIBAIRSPYSRC/src/iqconverter_int16.h\
  $$LIBAIRSPYSRC/src/filters.h

CONFIG(MINGW32):LIBS += -LD:\libusb-1.0.19\MinGW32\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
