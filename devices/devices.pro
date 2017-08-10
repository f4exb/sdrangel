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
DEFINES += USE_SSSE3=1
QMAKE_CXXFLAGS += -mssse3
DEFINES += USE_SSE4_1=1
QMAKE_CXXFLAGS += -msse4.1

CONFIG(MINGW32):LIBBLADERFSRC = "D:\softs\bladeRF\host\libraries\libbladeRF\include"
CONFIG(MINGW64):LIBBLADERFSRC = "D:\softs\bladeRF\host\libraries\libbladeRF\include"
CONFIG(macx):LIBHACKRFSRC = "/opt/local/include"
CONFIG(MINGW32):LIBHACKRFSRC = "D:\softs\hackrf\host"
CONFIG(MINGW64):LIBHACKRFSRC = "D:\softs\hackrf\host"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../sdrbase
INCLUDEPATH += $$LIBBLADERFSRC
INCLUDEPATH += $$LIBHACKRFSRC
CONFIG(MINGW64)INCLUDEPATH += "D:\boost_1_58_0"

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

!macx:SOURCES += bladerf/devicebladerf.cpp\
        bladerf/devicebladerfvalues.cpp\
        bladerf/devicebladerfshared.cpp

SOURCES += hackrf/devicehackrf.cpp\
        hackrf/devicehackrfvalues.cpp\
        hackrf/devicehackrfshared.cpp

!macx:HEADERS -= bladerf/devicebladerf.h\
        bladerf/devicebladerfparam.h\
        bladerf/devicebladerfvalues.h\
        bladerf/devicebladerfshared.h

HEADERS  += hackrf/devicehackrf.h\
        hackrf/devicehackrfparam.h\
        hackrf/devicehackrfvalues.h\
        hackrf/devicehackrfshared.h

LIBS += -L../sdrbase/$${build_subdir} -lsdrbase
LIBS += -L../libbladerf/$${build_subdir} -llibbladerf
LIBS += -L../libhackrf/$${build_subdir} -llibhackrf

macx {
    LIBS -= -L../libbladerf/$${build_subdir} -llibbladerf
    LIBS -= -L../libhackrf/$${build_subdir} -llibhackrf
    LIBS += -L/opt/local/lib -lhackrf
}
