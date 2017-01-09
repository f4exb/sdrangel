project(rtlsdr)

add_definitions(-DDETACH_KERNEL_DRIVER=ON)

find_package(LibUSB)

set(rtlsdr_SOURCES
    ${LIBRTLSDRSRC}/src/librtlsdr.c 
    ${LIBRTLSDRSRC}/src/tuner_e4k.c 
    ${LIBRTLSDRSRC}/src/tuner_fc0012.c  
    ${LIBRTLSDRSRC}/src/tuner_fc0013.c  
    ${LIBRTLSDRSRC}/src/tuner_fc2580.c  
    ${LIBRTLSDRSRC}/src/tuner_r82xx.c   
    ${LIBRTLSDRSRC}/src/getopt/getopt.c 
)

set(rtlsdr_HEADERS
    ${LIBRTLSDRSRC}/include/rtl-sdr.h   
    ${LIBRTLSDRSRC}/include/rtl-sdr_export.h    
    ${LIBRTLSDRSRC}/src/getopt/getopt.h 
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBRTLSDRSRC}/include
    ${LIBRTLSDRSRC}/src
)

add_definitions(-DQT_SHARED)

add_library(rtlsdr SHARED
    ${rtlsdr_SOURCES}
)

target_link_libraries(rtlsdr
    ${LIBUSB_LIBRARIES}
)

install(TARGETS rtlsdr DESTINATION lib)
