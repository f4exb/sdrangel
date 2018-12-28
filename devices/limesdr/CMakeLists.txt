project(limesdrdevice)

set(limesdrdevice_SOURCES
    devicelimesdr.cpp
    devicelimesdrparam.cpp
    devicelimesdrshared.cpp
)

set(limesdrdevice_HEADERS
    devicelimesdr.h
    devicelimesdrparam.h
    devicelimesdrshared.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBLIMESUITESRC}/src
    ${LIBLIMESUITESRC}/src/ADF4002
    ${LIBLIMESUITESRC}/src/ConnectionRegistry
    ${LIBLIMESUITESRC}/src/FPGA_common
    ${LIBLIMESUITESRC}/src/GFIR
    ${LIBLIMESUITESRC}/src/lms7002m
    ${LIBLIMESUITESRC}/src/lms7002m_mcu
    ${LIBLIMESUITESRC}/src/Si5351C
    ${LIBLIMESUITESRC}/src/protocols    
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIMESUITE_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

add_library(limesdrdevice SHARED
    ${limesdrdevice_SOURCES}
)

set_target_properties(limesdrdevice PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

if (BUILD_DEBIAN)
target_link_libraries(limesdrdevice
    limesuite
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(limesdrdevice
    ${LIMESUITE_LIBRARY}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS limesdrdevice DESTINATION lib)
