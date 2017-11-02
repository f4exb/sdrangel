#--------------------------------------------------------
#
# Pro file for Android and Windows builds with Qt Creator
#
#--------------------------------------------------------

QT += core

TEMPLATE = lib
TARGET = liblimesuite

DEFINES += ENOLINK=21

QMAKE_CXXFLAGS += -fpermissive
QMAKE_CXXFLAGS += -std=c++11

CONFIG(MINGW32):LIBLIMESUITESRC = "D:\softs\LimeSuite"
CONFIG(MINGW64):LIBLIMESUITESRC = "D:\softs\LimeSuite"

CONFIG(MINGW32):INCLUDEPATH += "D:\softs\libusb-1.0.20\include"
CONFIG(MINGW64):INCLUDEPATH += "D:\softs\libusb-1.0.20\include"

CONFIG(MINGW32):INCLUDEPATH += "..\libsqlite3\src"
CONFIG(MINGW64):INCLUDEPATH += "..\libsqlite3\src"

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
    
SOURCES = $$LIBLIMESUITESRC/src/ADF4002/ADF4002.cpp\
    $$LIBLIMESUITESRC/src/API/lms7_api.cpp\
    $$LIBLIMESUITESRC/src/API/lms7_device.cpp\
    $$LIBLIMESUITESRC/src/API/LimeSDR_mini.cpp\
    $$LIBLIMESUITESRC/src/API/qLimeSDR.cpp\
    src/BuiltinConnections.cpp\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/ConnectionHandle.cpp\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/ConnectionRegistry.cpp\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/IConnection.cpp\
    $$LIBLIMESUITESRC/src/ConnectionSTREAM/ConnectionSTREAM.cpp\
    $$LIBLIMESUITESRC/src/ConnectionSTREAM/ConnectionSTREAMImages.cpp\
    $$LIBLIMESUITESRC/src/ConnectionSTREAM/ConnectionSTREAMing.cpp\
    $$LIBLIMESUITESRC/src/ConnectionSTREAM/ConnectionSTREAMEntry.cpp\
    $$LIBLIMESUITESRC/src/Connection_uLimeSDR/Connection_uLimeSDR.cpp\
    $$LIBLIMESUITESRC/src/Connection_uLimeSDR/Connection_uLimeSDRing.cpp\
    $$LIBLIMESUITESRC/src/Connection_uLimeSDR/Connection_uLimeSDREntry.cpp\
    $$LIBLIMESUITESRC/src/ConnectionXillybus/ConnectionXillybus.cpp\
    $$LIBLIMESUITESRC/src/ConnectionXillybus/ConnectionXillybusEntry.cpp\
    $$LIBLIMESUITESRC/src/ConnectionXillybus/ConnectionXillybusing.cpp\
    $$LIBLIMESUITESRC/src/FPGA_common/FPGA_common.cpp\
    $$LIBLIMESUITESRC/src/GFIR/corrections.c\
    $$LIBLIMESUITESRC/src/GFIR/gfir_lms.c\
    $$LIBLIMESUITESRC/src/GFIR/lms.c\
    $$LIBLIMESUITESRC/src/GFIR/recipes.c\
    $$LIBLIMESUITESRC/src/GFIR/rounding.c\
    $$LIBLIMESUITESRC/src/kissFFT/kiss_fft.c\
    $$LIBLIMESUITESRC/src/lms7002m/CalibrationCache.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/goert.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_BaseCalibrations.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_filtersCalibration.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_gainCalibrations.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_parameters.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_RegistersMap.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_RxTxCalibrations.cpp\
    $$LIBLIMESUITESRC/src/lms7002m/mcu_dc_iq_calibration.cpp\
    $$LIBLIMESUITESRC/src/lms7002m_mcu/MCU_BD.cpp\
    $$LIBLIMESUITESRC/src/protocols/ILimeSDRStreaming.cpp\
    $$LIBLIMESUITESRC/src/protocols/LMS64CProtocol.cpp\
    $$LIBLIMESUITESRC/src/Si5351C/Si5351C.cpp\
    $$LIBLIMESUITESRC/src/ErrorReporting.cpp\
    $$LIBLIMESUITESRC/src/Logger.cpp\
    src/SystemResources.cpp\
    src/VersionInfo.cpp
    
HEADERS = $$LIBLIMESUITESRC/src/ADF4002/ADF4002.h\
    $$LIBLIMESUITESRC/src/API/lms7_device.h\
    $$LIBLIMESUITESRC/src/API/LimeSDR_mini.h\
    $$LIBLIMESUITESRC/src/API/qLimeSDR.h\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/ConnectionHandle.h\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/ConnectionRegistry.h\
    $$LIBLIMESUITESRC/src/ConnectionRegistry/IConnection.h\
    $$LIBLIMESUITESRC/src/ConnectionSTREAM/ConnectionSTREAM.h\
    $$LIBLIMESUITESRC/src/Connection_uLimeSDR/Connection_uLimeSDR.h\
    $$LIBLIMESUITESRC/src/Connection_uLimeSDR/DRV_DriverInterface.h\
    $$LIBLIMESUITESRC/src/Connection_uLimeSDR/FTD3XXLibrary/FTD3XX.h\
    $$LIBLIMESUITESRC/src/ConnectionXillybus/ConnectionXillybus.h\
    $$LIBLIMESUITESRC/src/FPGA_common/FPGA_common.h\
    $$LIBLIMESUITESRC/src/GFIR/dfilter.h\
    $$LIBLIMESUITESRC/src/GFIR/lms_gfir.h\
    $$LIBLIMESUITESRC/src/GFIR/lms.h\
    $$LIBLIMESUITESRC/src/kissFFT/_kiss_fft_guts.h\
    $$LIBLIMESUITESRC/src/kissFFT/kiss_fft.h\
    $$LIBLIMESUITESRC/src/lms7002m/CalibrationCache.h\
    $$LIBLIMESUITESRC/src/lms7002m/goertzel.h\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M.h\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_parameters.h\
    $$LIBLIMESUITESRC/src/lms7002m/LMS7002M_RegistersMap.h\
    $$LIBLIMESUITESRC/src/lms7002m/mcu_programs.h\
    $$LIBLIMESUITESRC/src/lms7002m_mcu/MCU_BD.h\
    $$LIBLIMESUITESRC/src/lms7002m_mcu/MCU_File.h\
    $$LIBLIMESUITESRC/src/protocols/ADCUnits.h\
    $$LIBLIMESUITESRC/src/protocols/dataTypes.h\
    $$LIBLIMESUITESRC/src/protocols/fifo.h\
    $$LIBLIMESUITESRC/src/protocols/ILimeSDRStreaming.h\
    $$LIBLIMESUITESRC/src/protocols/LMS64CCommands.h\
    $$LIBLIMESUITESRC/src/protocols/LMS64CProtocol.h\
    $$LIBLIMESUITESRC/src/protocols/LMSBoards.h\
    $$LIBLIMESUITESRC/src/Si5351C/Si5351C.h\
    $$LIBLIMESUITESRC/src/ErrorReporting.h\
    $$LIBLIMESUITESRC/src/Logger.h\
    $$LIBLIMESUITESRC/src/SystemResources.h\
    $$LIBLIMESUITESRC/src/VersionInfo.h\
    $$LIBLIMESUITESRC/src/lime/LimeSuite.h
    
CONFIG(MINGW32):LIBS += -LD:\softs\libusb-1.0.20\MinGW32\dll -llibusb-1.0
CONFIG(MINGW64):LIBS += -LD:\softs\libusb-1.0.20\MinGW64\dll -llibusb-1.0

CONFIG(MINGW32):LIBS += -L../libsqlite3/release -llibsqlite3
CONFIG(MINGW64):LIBS += -L../libsqlite3/release -llibsqlite3

CONFIG(ANDROID):CONFIG += mobility
CONFIG(ANDROID):MOBILITY =
    
