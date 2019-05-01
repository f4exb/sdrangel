project(airspyhf)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(LibUSB)

set(airspyhf_SOURCES
    ${LIBAIRSPYHFSRC}/libairspyhf/src/airspyhf.c
    ${LIBAIRSPYHFSRC}/libairspyhf/src/iqbalancer.c
)

set(airspyhf_HEADERS
    ${LIBAIRSPYHFSRC}/libairspyhf/src/airspyhf.h
    ${LIBAIRSPYHFSRC}/libairspyhf/src/airspyhf_commands.h
    ${LIBAIRSPYHFSRC}/libairspyhf/src/iqbalancer.h
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

install(TARGETS airspyhf DESTINATION ${INSTALL_LIB_DIR})
