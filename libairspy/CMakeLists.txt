project(airspy)

find_package(LibUSB)

remove_definitions(-DUSE_SSE2)

set(airspy_SOURCES
    ${LIBAIRSPYSRC}/libairspy/src/airspy.c
    ${LIBAIRSPYSRC}/libairspy/src/iqconverter_float.c
    ${LIBAIRSPYSRC}/libairspy/src/iqconverter_int16.c
)

set(airspy_HEADERS
    ${LIBAIRSPYSRC}/libairspy/src/airspy.h
    ${LIBAIRSPYSRC}/libairspy/src/airspy_commands.h
    ${LIBAIRSPYSRC}/libairspy/src/iqconverter_float.h
    ${LIBAIRSPYSRC}/libairspy/src/iqconverter_int16.h
    ${LIBAIRSPYSRC}/libairspy/src/filters.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}    
    ${LIBAIRSPYSRC}/libairspy/src
)

add_definitions(-DQT_SHARED)

add_library(airspy SHARED
    ${airspy_SOURCES}
)

target_link_libraries(airspy
    ${LIBUSB_LIBRARIES}
)

install(TARGETS airspy DESTINATION lib)
