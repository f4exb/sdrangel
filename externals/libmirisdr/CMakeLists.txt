project(mirisdr)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_definitions(-DDETACH_KERNEL_DRIVER=ON)

find_package(LibUSB)

set(mirisdr_SOURCES
    ${LIBMIRISDRSRC}/src/libmirisdr.c 
    ${LIBMIRISDRSRC}/src/getopt/getopt.c 
)

set(mirisdr_HEADERS
    ${LIBMIRISDRSRC}/include/mirisdr.h
    ${LIBMIRISDRSRC}/include/mirisdr_export.h
    ${LIBMIRISDRSRC}/src/getopt/getopt.h
    ${LIBMIRISDRSRC}/src/convert/252_s16.c
    ${LIBMIRISDRSRC}/src/convert/336_s16.c
    ${LIBMIRISDRSRC}/src/convert/384_s16.c
    ${LIBMIRISDRSRC}/src/convert/504_s16.c
    ${LIBMIRISDRSRC}/src/convert/504_s8.c
    ${LIBMIRISDRSRC}/src/convert/base.c
    ${LIBMIRISDRSRC}/src/adc.c
    ${LIBMIRISDRSRC}/src/async.c
    ${LIBMIRISDRSRC}/src/constants.h
    ${LIBMIRISDRSRC}/src/convenience.c
    ${LIBMIRISDRSRC}/src/devices.c
    ${LIBMIRISDRSRC}/src/gain.c
    ${LIBMIRISDRSRC}/src/gain.h
    ${LIBMIRISDRSRC}/src/hard.c
    ${LIBMIRISDRSRC}/src/hard.h
    ${LIBMIRISDRSRC}/src/reg.c
    ${LIBMIRISDRSRC}/src/soft.c
    ${LIBMIRISDRSRC}/src/soft.h
    ${LIBMIRISDRSRC}/src/streaming.c
    ${LIBMIRISDRSRC}/src/structs.h
    ${LIBMIRISDRSRC}/src/sync.c
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBMIRISDRSRC}/include
    ${LIBMIRISDRSRC}/src
)

add_definitions(-DQT_SHARED)

add_library(mirisdr SHARED
    ${mirisdr_SOURCES}
)

target_link_libraries(mirisdr
    ${LIBUSB_LIBRARIES}
)

install(TARGETS mirisdr DESTINATION ${INSTALL_LIB_DIR})
