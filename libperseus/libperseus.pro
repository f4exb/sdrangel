#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = libperseus

DEFINES += HAVE_CONFIG_H=1

CONFIG(MINGW32):LIBPERSEUSSRC = "C:\softs\libperseus-sdr"
CONFIG(MINGW64):LIBPERSEUSSRC = "C:\softs\libperseus-sdr"
INCLUDEPATH += $$LIBPERSEUSSRC/src

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.20\include"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.20\include"

SOURCES = fpga_data.c\
  $$LIBPERSEUSSRC/fifo.c\
  $$LIBPERSEUSSRC/perseuserr.c\
  $$LIBPERSEUSSRC/perseusfx2.c\
  $$LIBPERSEUSSRC/perseus-in.c\
  $$LIBPERSEUSSRC/perseus-sdr.c

HEADERS = fpga_data.h\
  config.h\
  config.windows.h\
  $$LIBPERSEUSSRC/fifo.h\
  $$LIBPERSEUSSRC/perseuserr.h\
  $$LIBPERSEUSSRC/perseusfx2.h\
  $$LIBPERSEUSSRC/perseus-in.h\
  $$LIBPERSEUSSRC/perseus-sdr.h

CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
