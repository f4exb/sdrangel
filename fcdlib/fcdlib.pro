#--------------------------------------------
#
# Pro file for Windows build with Qt Creator
#
#--------------------------------------------

QT += core

TEMPLATE = lib
TARGET = fcdlib

CONFIG(MINGW32):INCLUDEPATH += "D:\libusb-1.0.19\include"

SOURCES = $$PWD/fcdtraits.cpp\
    $$PWD/fcdproplusconst.cpp\
    $$PWD/fcdproconst.cpp

HEADERS = $$PWD/fcdtraits.h\
    $$PWD/fcdproplusconst.h\
    $$PWD/fcdproconst.h

CONFIG(MINGW32):LIBS += -LD:\libusb-1.0.19\MinGW32\dll -llibusb-1.0
