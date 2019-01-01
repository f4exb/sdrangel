#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libairspy

CONFIG(MINGW32):LIBAIRSPYSRC = "C:\softs\libairspy\libairspy"
CONFIG(MINGW64):LIBAIRSPYSRC = "C:\softs\libairspy\libairspy"
CONFIG(MSVC):LIBAIRSPYSRC = "C:\softs\libairspy\libairspy"

INCLUDEPATH += $$LIBAIRSPYSRC/src

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.22\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.22\include\libusb-1.0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\libusb-1.0.22\include\libusb-1.0"
CONFIG(MSVC):INCLUDEPATH += "C:\softs\pthreads-w32\include"
CONFIG(macx):INCLUDEPATH += "/opt/local/include"

CONFIG(MSVC):DEFINES += _TIMESPEC_DEFINED

SOURCES = $$LIBAIRSPYSRC/src/airspy.c\
  $$LIBAIRSPYSRC/src/iqconverter_float.c\
  $$LIBAIRSPYSRC/src/iqconverter_int16.c

HEADERS = $$LIBAIRSPYSRC/src/airspy.h\
  $$LIBAIRSPYSRC/src/airspy_commands.h\
  $$LIBAIRSPYSRC/src/iqconverter_float.h\
  $$LIBAIRSPYSRC/src/iqconverter_int16.h\
  $$LIBAIRSPYSRC/src/filters.h

CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.22\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.22\MinGW64\dll -llibusb-1.0
CONFIG(MSVC):LIBS += -LC:\softs\libusb-1.0.22\MS64\dll -llibusb-1.0
CONFIG(MSVC):LIBS += -LC:\softs\pthreads-w32\lib\x64 -lpthreadVC2
macx {
    SOURCES =
    HEADERS =
    LIBS += -L/opt/local/lib -lairspy
}

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
