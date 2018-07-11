#--------------------------------------------
#
# Pro file for Windows build with Qt Creator
#
#--------------------------------------------

QT += core

TEMPLATE = lib
TARGET = fcdlib

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.20\include"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.20\include"

SOURCES = $$PWD/fcdtraits.cpp\
    $$PWD/fcdproplusconst.cpp\
    $$PWD/fcdproconst.cpp

HEADERS = $$PWD/fcdtraits.h\
    $$PWD/fcdproplusconst.h\
    $$PWD/fcdproconst.h

CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0
