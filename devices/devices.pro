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
CONFIG(macx):LIBHACKRFSRC = "/opt/local/include"
CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host"
CONFIG(MINGW64):LIBHACKRFSRC = "D:\softs\hackrf\host"
CONFIG(MINGW32):LIBLIMESUITESRC = "D:\softs\LimeSuite"
CONFIG(MINGW64):LIBLIMESUITESRC = "D:\softs\LimeSuite"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../sdrbase
INCLUDEPATH += $$LIBBLADERFSRC
INCLUDEPATH += $$LIBHACKRFSRC
CONFIG(MINGW64)INCLUDEPATH += ../liblimesuite/srcmw
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/ADF4002
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/ConnectionRegistry
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/FPGA_common
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/GFIR
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/lms7002m
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/lms7002m_mcu
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/Si5351C
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/src/protocols
CONFIG(MINGW64)INCLUDEPATH += $$LIBLIMESUITESRC/external/cpp-feather-ini-parser

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

!macx:SOURCES += bladerf/devicebladerf.cpp\
        bladerf/devicebladerfvalues.cpp

SOURCES += hackrf/devicehackrf.cpp\
        hackrf/devicehackrfvalues.cpp

CONFIG(MINGW64)SOURCES += limesdr/devicelimesdr.cpp\
        limesdr/devicelimesdrparam.cpp\
        limesdr/devicelimesdrshared.cpp

!macx:HEADERS -= bladerf/devicebladerf.h\
        bladerf/devicebladerfparam.h\
        bladerf/devicebladerfvalues.h

HEADERS  += hackrf/devicehackrf.h\
        hackrf/devicehackrfparam.h\
        hackrf/devicehackrfvalues.h\

CONFIG(MINGW64)HEADERS += limesdr/devicelimesdr.h\
        limesdr/devicelimesdrparam.h\
        limesdr/devicelimesdrshared.h

LIBS += -L../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../libbladerf/$${build_subdir} -llibbladerf
LIBS += -L../libhackrf/$${build_subdir} -llibhackrf
CONFIG(MINGW64)LIBS += -L../liblimesuite/$${build_subdir} -lliblimesuite

macx {
    LIBS -= -L../libbladerf/$${build_subdir} -llibbladerf
    LIBS -= -L../libhackrf/$${build_subdir} -llibhackrf
    LIBS += -L/opt/local/lib -lhackrf
}
