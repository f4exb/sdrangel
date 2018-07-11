#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = liblimesuite

DEFINES += ENOLINK=21
DEFINES += "__unix__"

QMAKE_CXXFLAGS += -fpermissive
QMAKE_CXXFLAGS += -std=c++11

CONFIG(MINGW32):LIBLIMESUITESRC = "C:\softs\LimeSuite"
CONFIG(MINGW64):LIBLIMESUITESRC = "C:\softs\LimeSuite"

CONFIG(MINGW32):INCLUDEPATH += "C:\softs\libusb-1.0.20\include\libusb-1.0"
CONFIG(MINGW64):INCLUDEPATH += "C:\softs\libusb-1.0.20\include"

#CONFIG(MINGW32):INCLUDEPATH += "..\libsqlite3\src"
#CONFIG(MINGW64):INCLUDEPATH += "..\libsqlite3\src"

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
    
SOURCES = $$LIBLIMESUITESRC/src/Logger.cpp\
    $$LIBLIMESUITESRC/src/ADF4002/ADF4002.cpp\
    $$LIBLIMESUITESRC/src/lms7002m_mcu/MCU_BD.cpp\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/IConnection.cpp\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/ConnectionHandle.cpp\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/ConnectionRegistry.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_RegistersMap.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_parameters.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_RxTxCalibrations.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_BaseCalibrations.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/goert.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/mcu_dc_iq_calibration.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_filtersCalibration.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_gainCalibrations.cpp\
    $$LIBLIMESUITESRC/src/protocols/LMS64CProtocol.cpp\
    $$LIBLIMESUITESRC/src/protocols/Streamer.cpp\
    $$LIBLIMESUITESRC/src/protocols/ConnectionImages.cpp\
    $$LIBLIMESUITESRC/src/Si5351C/Si5351C.cpp\
    $$LIBLIMESUITESRC/src/API/lms7_api.cpp\
    $$LIBLIMESUITESRC/src/API/lms7_device.cpp\
    $$LIBLIMESUITESRC/src/API/LmsGeneric.cpp\
    $$LIBLIMESUITESRC/src/API/qLimeSDR.cpp\
    $$LIBLIMESUITESRC/src/API/LimeSDR_mini.cpp\
    $$LIBLIMESUITESRC/src/API/LimeSDR.cpp\
    $$LIBLIMESUITESRC/src/FPGA_common/FPGA_common.cpp\
    $$LIBLIMESUITESRC/src/FPGA_common/FPGA_Mini.cpp\
    $$LIBLIMESUITESRC/src/FPGA_common/FPGA_Q.cpp\
    $$LIBLIMESUITESRC/src/GFIR/corrections.c\
    $$LIBLIMESUITESRC/src/GFIR/gfir_lms.c\
    $$LIBLIMESUITESRC/src/GFIR/lms.c\
    $$LIBLIMESUITESRC/src/GFIR/recipes.c\
    $$LIBLIMESUITESRC/src/GFIR/rounding.c\
    $$LIBLIMESUITESRC/src/windowFunction.cpp\
    $$LIBLIMESUITESRC/src/ConnectionFTDI/ConnectionFT601.cpp\
    $$LIBLIMESUITESRC/src/ConnectionFTDI//ConnectionFT601Entry.cpp\
    $$LIBLIMESUITESRC/src/ConnectionFX3/ConnectionFX3Entry.cpp\
    $$LIBLIMESUITESRC/src/ConnectionFX3/ConnectionFX3.cpp\    
    src/BuiltinConnections.cpp\
    src/SystemResources.cpp\
    src/VersionInfo.cpp
    
HEADERS = $$LIBLIMESUITESRC/src/API/*.h\
    $$LIBLIMESUITESRC/src/GFIR/*.h\
    $$LIBLIMESUITESRC/src/protocols/*.h\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/*.h\
    $$LIBLIMESUITESRC/src/lms7002m_mcu/*.h\
    $$LIBLIMESUITESRC/src/ADF4002/*.h\
    $$LIBLIMESUITESRC/src/Si5351C/*.h\
    $$LIBLIMESUITESRC/src/lms7002m/*.h\
    $$LIBLIMESUITESRC/src/FPGA_common/*.h\
    $$LIBLIMESUITESRC/src/HPM7/*.h
    
CONFIG(MINGW32):LIBS += -LC:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LC:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

#CONFIG(MINGW32):LIBS += -L../libsqlite3/release -llibsqlite3
#CONFIG(MINGW64):LIBS += -L../libsqlite3/release -llibsqlite3

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
    
