project(airspyhf)

find_package(LibUSB)

set(airspyhf_SOURCES
    ${LIBAIRSPYHFSRC}/libairspyhf/src/airspyhf.c
    ${LIBAIRSPYHFSRC}/libairspyhf/src/iqbalancer.c
    ${LIBAIRSPYHFSRC}/libairspyhf/src/iqconverter_int16.c
)

set(airspyhf_HEADERS
    ${LIBAIRSPYHFSRC}/libairspyhf/src/airspyhf.h
    ${LIBAIRSPYHFSRC}/libairspyhf/src/airspyhf_commands.h
    ${LIBAIRSPYHFSRC}/libairspyhf/src/iqbalancer.h
    ${LIBAIRSPYHFSRC}/libairspyhf/src/iqconverter_int16.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}    
    ${LIBAIRSPYHFSRC}/libairspyhf/src
)

add_definitions(-DQT_SHARED)

add_library(airspyhf SHARED
    ${airspyhf_SOURCES}
)

target_link_libraries(airspyhf
    ${LIBUSB_LIBRARIES}
)

install(TARGETS airspyhf DESTINATION lib)
