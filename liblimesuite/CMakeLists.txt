project(limesuite)

find_package(LibUSB)

set(limesuite_SOURCES
	${LIBLIMESUITESRC}/src/Logger.cpp
    ${LIBLIMESUITESRC}/src/ADF4002/ADF4002.cpp
    ${LIBLIMESUITESRC}/src/lms7002m_mcu/MCU_BD.cpp
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/IConnection.cpp
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/ConnectionHandle.cpp
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/ConnectionRegistry.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_RegistersMap.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_parameters.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_RxTxCalibrations.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_BaseCalibrations.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/goert.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/mcu_dc_iq_calibration.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_filtersCalibration.cpp
    ${LIBLIMESUITESRC}/src/lms7002m/LMS7002M_gainCalibrations.cpp
    ${LIBLIMESUITESRC}/src/protocols/LMS64CProtocol.cpp
    ${LIBLIMESUITESRC}/src/protocols/Streamer.cpp
    ${LIBLIMESUITESRC}/src/protocols/ConnectionImages.cpp
    ${LIBLIMESUITESRC}/src/Si5351C/Si5351C.cpp
    ${LIBLIMESUITESRC}/src/API/lms7_api.cpp
    ${LIBLIMESUITESRC}/src/API/lms7_device.cpp
    ${LIBLIMESUITESRC}/src/API/LmsGeneric.cpp
    ${LIBLIMESUITESRC}/src/API/qLimeSDR.cpp
    ${LIBLIMESUITESRC}/src/API/LimeSDR_mini.cpp
    ${LIBLIMESUITESRC}/src/API/LimeSDR.cpp
    ${LIBLIMESUITESRC}/src/FPGA_common/FPGA_common.cpp
    ${LIBLIMESUITESRC}/src/FPGA_common/FPGA_Mini.cpp
    ${LIBLIMESUITESRC}/src/FPGA_common/FPGA_Q.cpp
    ${LIBLIMESUITESRC}/src/GFIR/corrections.c
    ${LIBLIMESUITESRC}/src/GFIR/gfir_lms.c
    ${LIBLIMESUITESRC}/src/GFIR/lms.c
    ${LIBLIMESUITESRC}/src/GFIR/recipes.c
    ${LIBLIMESUITESRC}/src/GFIR/rounding.c
    ${LIBLIMESUITESRC}/src/windowFunction.cpp
    ${LIBLIMESUITESRC}/src/ConnectionFTDI/ConnectionFT601.cpp
    ${LIBLIMESUITESRC}/src/ConnectionFTDI//ConnectionFT601Entry.cpp
    ${LIBLIMESUITESRC}/src/ConnectionFX3/ConnectionFX3Entry.cpp
    ${LIBLIMESUITESRC}/src/ConnectionFX3/ConnectionFX3.cpp    
    src/BuiltinConnections.cpp
    src/SystemResources.cpp
    src/VersionInfo.cpp
)

set(limesuite_HEADERS
	${LIBLIMESUITESRC}/src/API/*.h
    ${LIBLIMESUITESRC}/src/GFIR/*.h
    ${LIBLIMESUITESRC}/src/protocols/*.h
    ${LIBLIMESUITESRC}/src/ConnectionRegistry/*.h
    ${LIBLIMESUITESRC}/src/lms7002m_mcu/*.h
    ${LIBLIMESUITESRC}/src/ADF4002/*.h
    ${LIBLIMESUITESRC}/src/Si5351C/*.h
    ${LIBLIMESUITESRC}/src/lms7002m/*.h
    ${LIBLIMESUITESRC}/src/FPGA_common/*.h
    ${LIBLIMESUITESRC}/src/HPM7/*.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBLIMESUITESRC}/src
    ${LIBLIMESUITESRC}/src/API
    ${LIBLIMESUITESRC}/src/GFIR
    ${LIBLIMESUITESRC}/src/protocols
    ${LIBLIMESUITESRC}/src/ConnectionRegistry
    ${LIBLIMESUITESRC}/src/lms7002m_mcu
    ${LIBLIMESUITESRC}/src/ADF4002
    ${LIBLIMESUITESRC}/src/Si5351C
    ${LIBLIMESUITESRC}/src/lms7002m
    ${LIBLIMESUITESRC}/src/FPGA_common
    ${LIBLIMESUITESRC}/src/HPM7
    ${LIBLIMESUITESRC}/src/kissFFT
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
)

install(TARGETS limesuite DESTINATION lib)


