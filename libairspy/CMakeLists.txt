project(airspy)

find_package(LibUSB)

set(airspy_SOURCES
    ${LIBAIRSPYSRC}/src/airspy.c
    ${LIBAIRSPYSRC}/src/iqconverter_float.c
    ${LIBAIRSPYSRC}/src/iqconverter_int16.c
)

set(airspy_HEADERS
    ${LIBAIRSPYSRC}/src/airspy.h
    ${LIBAIRSPYSRC}/src/airspy_commands.h
    ${LIBAIRSPYSRC}/src/iqconverter_float.h
    ${LIBAIRSPYSRC}/src/iqconverter_int16.h
    ${LIBAIRSPYSRC}/src/filters.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}    
    ${LIBAIRSPYSRC}/src
)

add_definitions(-DQT_SHARED)

add_library(airspy SHARED
    ${airspy_SOURCES}
)

target_link_libraries(airspy
    ${LIBUSB_LIBRARIES}
)

install(TARGETS airspy DESTINATION lib)
