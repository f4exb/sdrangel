project(rtlsdr)

add_definitions(-DDETACH_KERNEL_DRIVER=ON)

find_package(LibUSB)

set(rtlsdr_SOURCES
    ${LIBRTLSDRSRC}/src/librtlsdr.c 
    ${LIBRTLSDRSRC}/src/rtlsdr_rpc_msg.c
    ${LIBRTLSDRSRC}/src/rtlsdr_rpc.c
    ${LIBRTLSDRSRC}/src/tuner_e4k.c 
    ${LIBRTLSDRSRC}/src/tuner_fc0012.c  
    ${LIBRTLSDRSRC}/src/tuner_fc0013.c  
    ${LIBRTLSDRSRC}/src/tuner_fc2580.c  
    ${LIBRTLSDRSRC}/src/tuner_r82xx.c   
    ${LIBRTLSDRSRC}/src/getopt/getopt.c 
    ${LIBRTLSDRSRC}/src/convenience/convenience.c
)

set(rtlsdr_HEADERS
    ${LIBRTLSDRSRC}/include/reg_field.h   
    ${LIBRTLSDRSRC}/include/rtl-sdr_export.h    
    ${LIBRTLSDRSRC}/include/rtlsdr_i2c.h    
    ${LIBRTLSDRSRC}/include/rtlsdr_rpc_msg.h    
    ${LIBRTLSDRSRC}/include/rtlsdr_rpc.h    
    ${LIBRTLSDRSRC}/include/rtl-sdr.h   
    ${LIBRTLSDRSRC}/include/tuner_e4k.h
    ${LIBRTLSDRSRC}/include/tuner_fc0012.h  
    ${LIBRTLSDRSRC}/include/tuner_fc0013.h  
    ${LIBRTLSDRSRC}/include/tuner_fc2580.h 
    ${LIBRTLSDRSRC}/include/tuner_r82xx.h  
    ${LIBRTLSDRSRC}/include/getopt/getopt.h 
    ${LIBRTLSDRSRC}/src/convenience/convenience.h 
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
