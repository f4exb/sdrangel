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
QMAKE_CXXFLAGS += -std=c++11
macx:QMAKE_LFLAGS += -F/Library/Frameworks

CONFIG(MSVC):DEFINES += devices_EXPORTS

CONFIG(MINGW32):LIBBLADERF = "C:\Programs\bladeRF"
CONFIG(MINGW64):LIBBLADERF = "C:\Programs\bladeRF"

CONFIG(macx):LIBHACKRFSRC = "/opt/local/include"
CONFIG(MINGW32):LIBHACKRFSRC = "C:\softs\hackrf\host"
CONFIG(MINGW64):LIBHACKRFSRC = "C:\softs\hackrf\host"
CONFIG(MSVC):LIBHACKRFSRC = "C:\softs\hackrf\host"

CONFIG(macx):LIBLIMESUITESRC = "../../../LimeSuite-17.12.0"
CONFIG(MINGW32):LIBLIMESUITESRC = "C:\softs\LimeSuite"
CONFIG(MINGW64):LIBLIMESUITESRC = "C:\softs\LimeSuite"

CONFIG(MINGW32):LIBPERSEUSSRC = "C:\softs\libperseus-sdr"

CONFIG(macx):LIBIIOSRC = "../../../libiio"
CONFIG(MINGW32):LIBIIOSRC = "C:\softs\libiio"
CONFIG(MINGW64):LIBIIOSRC = "C:\softs\libiio"

INCLUDEPATH += $$PWD
INCLUDEPATH += ../exports
INCLUDEPATH += ../sdrbase
INCLUDEPATH += "C:\softs\boost_1_66_0"
INCLUDEPATH += "C:\softs\libusb-1.0.22\include"
INCLUDEPATH += $$LIBBLADERF/include
INCLUDEPATH += $$LIBHACKRFSRC

MINGW32 || MINGW64 || macx {
    INCLUDEPATH += ../liblimesuite/srcmw
    INCLUDEPATH += $$LIBLIMESUITESRC/src
    INCLUDEPATH += $$LIBLIMESUITESRC/src/ADF4002
    INCLUDEPATH += $$LIBLIMESUITESRC/src/ConnectionRegistry
    INCLUDEPATH += $$LIBLIMESUITESRC/src/FPGA_common
    INCLUDEPATH += $$LIBLIMESUITESRC/src/GFIR
    INCLUDEPATH += $$LIBLIMESUITESRC/src/lms7002m
    INCLUDEPATH += $$LIBLIMESUITESRC/src/lms7002m_mcu
    INCLUDEPATH += $$LIBLIMESUITESRC/src/Si5351C
    INCLUDEPATH += $$LIBLIMESUITESRC/src/protocols
    INCLUDEPATH += $$LIBLIMESUITESRC/external/cpp-feather-ini-parser
}

MSVC {
    INCLUDEPATH += "C:\Program Files\PothosSDR\include"
}

INCLUDEPATH += $$LIBPERSEUSSRC
!macx:INCLUDEPATH += $$LIBIIOSRC

CONFIG(Release):build_subdir = release
CONFIG(Debug):build_subdir = debug

MINGW32 || MINGW64 {
    SOURCES += bladerf1/devicebladerf1.cpp\
            bladerf1/devicebladerf1values.cpp\
            bladerf1/devicebladerf1shared.cpp

    SOURCES += bladerf2/devicebladerf2.cpp\
            bladerf2/devicebladerf2shared.cpp

    SOURCES += hackrf/devicehackrf.cpp\
            hackrf/devicehackrfvalues.cpp\
            hackrf/devicehackrfshared.cpp

    SOURCES += limesdr/devicelimesdr.cpp\
            limesdr/devicelimesdrparam.cpp\
            limesdr/devicelimesdrshared.cpp

    SOURCES += plutosdr/deviceplutosdr.cpp\
            plutosdr/deviceplutosdrbox.cpp\
            plutosdr/deviceplutosdrparams.cpp\
            plutosdr/deviceplutosdrscan.cpp\
            plutosdr/deviceplutosdrshared.cpp

    HEADERS += bladerf2/devicebladerf2.h\
            bladerf2/devicebladerf2shared.h

    HEADERS += bladerf1/devicebladerf1.h\
            bladerf1/devicebladerf1param.h\
            bladerf1/devicebladerf1values.h\
            bladerf1/devicebladerf1shared.h

    HEADERS  += hackrf/devicehackrf.h\
            hackrf/devicehackrfparam.h\
            hackrf/devicehackrfvalues.h\
            hackrf/devicehackrfshared.h

    HEADERS += limesdr/devicelimesdr.h\
            limesdr/devicelimesdrparam.h\
            limesdr/devicelimesdrshared.h

    HEADERS += plutosdr/deviceplutosdr.h\
            plutosdr/deviceplutosdrbox.h\
            plutosdr/deviceplutosdrparams.h\
            plutosdr/deviceplutosdrscan.h\
            plutosdr/deviceplutosdrshared.h
}

macx {
    SOURCES += hackrf/devicehackrf.cpp\
            hackrf/devicehackrfvalues.cpp\
            hackrf/devicehackrfshared.cpp

    SOURCES += limesdr/devicelimesdr.cpp\
            limesdr/devicelimesdrparam.cpp\
            limesdr/devicelimesdrshared.cpp

    HEADERS  += hackrf/devicehackrf.h\
            hackrf/devicehackrfparam.h\
            hackrf/devicehackrfvalues.h\
            hackrf/devicehackrfshared.h

    HEADERS += limesdr/devicelimesdr.h\
            limesdr/devicelimesdrparam.h\
            limesdr/devicelimesdrshared.h
}

MSVC {
    SOURCES += hackrf/devicehackrf.cpp\
            hackrf/devicehackrfvalues.cpp\
            hackrf/devicehackrfshared.cpp

    SOURCES += limesdr/devicelimesdr.cpp\
            limesdr/devicelimesdrparam.cpp\
            limesdr/devicelimesdrshared.cpp

    SOURCES += soapysdr/devicesoapysdr.cpp\
            soapysdr/devicesoapysdrparams.cpp\
            soapysdr/devicesoapysdrscan.cpp\
            soapysdr/devicesoapysdrshared.cpp

    HEADERS  += hackrf/devicehackrf.h\
            hackrf/devicehackrfparam.h\
            hackrf/devicehackrfvalues.h\
            hackrf/devicehackrfshared.h

    HEADERS += limesdr/devicelimesdr.h\
            limesdr/devicelimesdrparam.h\
            limesdr/devicelimesdrshared.h

    HEADERS += soapysdr/devicesoapysdr.h\
            soapysdr/devicesoapysdrparams.h\
            soapysdr/devicesoapysdrscan.h\
            soapysdr/devicesoapysdrshared.h
}

LIBS += -L../sdrbase/$${build_subdir} -lsdrbase

MINGW32 || MINGW64 {
    LIBS += -L$$LIBBLADERF/lib -lbladeRF
    LIBS += -L../libhackrf/$${build_subdir} -llibhackrf
    LIBS += -L../liblimesuite/$${build_subdir} -lliblimesuite
    LIBS += -L../libiio/$${build_subdir} -llibiio
}

macx {
    LIBS += -L/opt/local/lib -lhackrf
    LIBS += -L/usr/local/lib -lLimeSuite
    LIBS += -framework iio
}

MSVC {
    LIBS += -L../libhackrf/$${build_subdir} -llibhackrf
    LIBS += -L"C:\Program Files\PothosSDR\bin" -L"C:\Program Files\PothosSDR\lib" -lLimeSuite
    LIBS += -L"C:\Program Files\PothosSDR\bin" -L"C:\Program Files\PothosSDR\lib" -lSoapySDR
}
