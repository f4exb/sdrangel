#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core gui multimedia opengl

TEMPLATE = lib
TARGET = devices

DEFINES += USE_SSE2=1
QMAKE_CXXFLAGS += -msse2
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

CONFIG(MINGW32):LIBBLADERFSRC = "D:\softs\bladeRF\host\libraries\libbladeRF\include"
CONFIG(MINGW64):LIBBLADERFSRC = "D:\softs\bladeRF\host\libraries\libbladeRF\include"
CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host"
CONFIG(MINGW64):LIBHACKRFSRC = "D:\softs\hackrf\host"
INCLUDEPATH += $$PWD
INCLUDEPATH += ../../../sdrbase
INCLUDEPATH += $$LIBBLADERFSRC
INCLUDEPATH += $$LIBHACKRFSRC

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

SOURCES += bladerf/devicebladerf.cpp\
        bladerf/devicebladerfvalues.cpp\
        hackrf/devicehackrf.cpp\
        hackrf/devicehackrfvalues.cpp

HEADERS  += bladerf/devicebladerf.h\
        bladerf/devicebladerfparam.h\
        bladerf/devicebladerfvalues.h\
        hackrf/devicehackrf.h\
        hackrf/devicehackrfparam.h\
        hackrf/devicehackrfvalues.h

LIBS += -L../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../libbladerf/$${build_subdir} -llibbladerf
