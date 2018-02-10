#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libperseus

CONFIG(MINGW32):LIBPERSEUSSRC = "D:\softs\libperseus-sdr"
CONFIG(MINGW64):LIBPERSEUSSRC = "D:\softs\libperseus-sdr"
INCLUDEPATH += $$LIBPERSEUSSRC

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\libusb-1.0.20\include\libusb-1.0"

SOURCES = $$LIBPERSEUSSRC/fifo.c\
  $$LIBPERSEUSSRC/perseus24v11_512.c\
  $$LIBPERSEUSSRC/perseus24v41_512.c\
  $$LIBPERSEUSSRC/perseuserr.c\
  $$LIBPERSEUSSRC/perseusfx2.c\
  $$LIBPERSEUSSRC/perseus-in.c\
  $$LIBPERSEUSSRC/perseus-sdr.c

HEADERS = $$LIBPERSEUSSRC/fifo.h\
  $$LIBPERSEUSSRC/perseuserr.h\
  $$LIBPERSEUSSRC/perseusfx2.h\
  $$LIBPERSEUSSRC/perseus-in.h\
  $$LIBPERSEUSSRC/perseus-sdr.h

CONFIG(MINGW32):LIBS += -LD:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
