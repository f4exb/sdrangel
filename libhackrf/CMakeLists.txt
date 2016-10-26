project(hackrf)

find_package(LibUSB)

set(hackrf_SOURCES
    ${LIBHACKRFSRC}/libhackrf/src/hackrf.c
)

set(hackrf_HEADERS
    ${LIBHACKRFSRC}/libhackrf/src/hackrf.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}    
    ${LIBHACKRFSRC}/libhackrf/src
)

add_definitions(-DQT_SHARED)

add_library(hackrf SHARED
    ${hackrf_SOURCES}
)

target_link_libraries(hackrf
    ${LIBUSB_LIBRARIES}
)

install(TARGETS hackrf DESTINATION lib)
