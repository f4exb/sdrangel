project(limesuite)

find_package(LibUSB)
find_package(SQLite3)

set(limesuite_SOURCES
    ${LIBLIMESUITESRC}/src/ADF4002/ADF4002.cpp 
    ${LIBLIMESUITESRC}/src/API/lms7_api.cpp
    ${LIBLIMESUITESRC}/src/API/lms7_device.cpp
    ${LIBLIMESUITESRC}/src/API/LimeSDR_mini.cpp
    ${LIBLIMESUITESRC}/src/API/qLimeSDR.cpp
    src/BuiltinConnections.cpp
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/ConnectionHandle.cpp
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/ConnectionRegistry.cpp
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/IConnection.cpp
    ${LIBLIMESUITESRC}/src/ConnectionSTREAM/ConnectionSTREAM.cpp
    ${LIBLIMESUITESRC}/src/ConnectionSTREAM/ConnectionSTREAMEntry.cpp
    ${LIBLIMESUITESRC}/src/ConnectionSTREAM/ConnectionSTREAMImages.cpp
    ${LIBLIMESUITESRC}/src/ConnectionSTREAM/ConnectionSTREAMing.cpp
    ${LIBLIMESUITESRC}/src/Connection_uLimeSDR/Connection_uLimeSDR.cpp
    ${LIBLIMESUITESRC}/src/Connection_uLimeSDR/Connection_uLimeSDREntry.cpp
    ${LIBLIMESUITESRC}/src/Connection_uLimeSDR/Connection_uLimeSDRing.cpp
#    ${LIBLIMESUITESRC}/src/Connection_uLimeSDR/DRV_DriverInterface.cpp
    ${LIBLIMESUITESRC}/src/ConnectionXillybus/ConnectionXillybus.cpp
    ${LIBLIMESUITESRC}/src/ConnectionXillybus/ConnectionXillybusEntry.cpp
    ${LIBLIMESUITESRC}/src/ConnectionXillybus/ConnectionXillybusing.cpp
    ${LIBLIMESUITESRC}/src/FPGA_common/FPGA_common.cpp
    ${LIBLIMESUITESRC}/src/GFIR/corrections.c
    ${LIBLIMESUITESRC}/src/GFIR/gfir_lms.c
    ${LIBLIMESUITESRC}/src/GFIR/lms.c
    ${LIBLIMESUITESRC}/src/GFIR/recipes.c
    ${LIBLIMESUITESRC}/src/GFIR/rounding.c
    ${LIBLIMESUITESRC}/src/kissFFT/kiss_fft.c
    ${LIBLIMESUITESRC}/src/lms7002m/CalibrationCache.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/goert.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_BaseCalibrations.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_filtersCalibration.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_gainCalibrations.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_parameters.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_RegistersMap.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_RxTxCalibrations.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/mcu_dc_iq_calibration.cpp
    ${LIBLIMESUITESRC}/src/lms7002m_mcu/MCU_BD.cpp
    ${LIBLIMESUITESRC}/src/protocols/ILimeSDRStreaming.cpp
    ${LIBLIMESUITESRC}/src/protocols/LMS64CProtocol.cpp
    ${LIBLIMESUITESRC}/src/Si5351C/Si5351C.cpp
    ${LIBLIMESUITESRC}/src/ErrorReporting.cpp
    ${LIBLIMESUITESRC}/src/Logger.cpp
    src/SystemResources.cpp
    src/VersionInfo.cpp
)

set(limesuite_HEADERS
    ${LIBLIMESUITESRC}/src/ADF4002/ADF4002.h 
    ${LIBLIMESUITESRC}/src/API/lms7_device.h
    ${LIBLIMESUITESRC}/src/API/LimeSDR_mini.h    
    ${LIBLIMESUITESRC}/src/API/qLimeSDR.h
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/ConnectionHandle.h
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/ConnectionRegistry.h
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/IConnection.h
    ${LIBLIMESUITESRC}/src/ConnectionSTREAM/ConnectionSTREAM.h
    ${LIBLIMESUITESRC}/src/Connection_uLimeSDR/Connection_uLimeSDR.h
#    ${LIBLIMESUITESRC}/src/Connection_uLimeSDR/DRV_DriverInterface.h
    ${LIBLIMESUITESRC}/src/Connection_uLimeSDR/FTD3XXLibrary/FTD3XX.h
    ${LIBLIMESUITESRC}/src/ConnectionXillybus/ConnectionXillybus.h
    ${LIBLIMESUITESRC}/src/FPGA_common/FPGA_common.h
    ${LIBLIMESUITESRC}/src/GFIR/dfilter.h
    ${LIBLIMESUITESRC}/src/GFIR/lms_gfir.h
    ${LIBLIMESUITESRC}/src/GFIR/lms.h
    ${LIBLIMESUITESRC}/src/kissFFT/_kiss_fft_guts.h
    ${LIBLIMESUITESRC}/src/kissFFT/kiss_fft.h
    ${LIBLIMESUITESRC}/src/lms7002m/CalibrationCache.h
    ${LIBLIMESUITESRC}/src/lms7002m/goertzel.h
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M.h
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_parameters.h
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_RegistersMap.h
    ${LIBLIMESUITESRC}/src/lms7002m/mcu_programs.h
    ${LIBLIMESUITESRC}/src/lms7002m_mcu/MCU_BD.h
    ${LIBLIMESUITESRC}/src/lms7002m_mcu/MCU_File.h
    ${LIBLIMESUITESRC}/src/protocols/ADCUnits.h
    ${LIBLIMESUITESRC}/src/protocols/dataTypes.h
    ${LIBLIMESUITESRC}/src/protocols/fifo.h
    ${LIBLIMESUITESRC}/src/protocols/ILimeSDRStreaming.h
    ${LIBLIMESUITESRC}/src/protocols/LMS64CCommands.h
    ${LIBLIMESUITESRC}/src/protocols/LMS64CProtocol.h
    ${LIBLIMESUITESRC}/src/protocols/LMSBoards.h
    ${LIBLIMESUITESRC}/src/Si5351C/Si5351C.h
    ${LIBLIMESUITESRC}/src/ErrorReporting.h
    ${LIBLIMESUITESRC}/src/Logger.h
    ${LIBLIMESUITESRC}/src/SystemResources.h
    ${LIBLIMESUITESRC}/src/VersionInfo.h
    ${LIBLIMESUITESRC}/src/lime/LimeSuite.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBLIMESUITESRC}/src
    ${LIBLIMESUITESRC}/src/ADF4002
    ${LIBLIMESUITESRC}/src/ConnectionRegistry
    ${LIBLIMESUITESRC}/src/FPGA_common
    ${LIBLIMESUITESRC}/src/GFIR
    ${LIBLIMESUITESRC}/src/lms7002m
    ${LIBLIMESUITESRC}/src/lms7002m_mcu
    ${LIBLIMESUITESRC}/src/Si5351C
    ${LIBLIMESUITESRC}/src/protocols
    ${LIBLIMESUITESRC}/external/cpp-feather-ini-parser
    ./include
)

set (CMAKE_CXX_STANDARD 11)
add_definitions(-DQT_SHARED)

add_library(limesuite SHARED
    ${limesuite_SOURCES}
)

target_link_libraries(limesuite
    ${LIBUSB_LIBRARIES}
    ${SQLITE3_LIBRARIES}
)

install(TARGETS limesuite DESTINATION lib)


