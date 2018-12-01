project(soapysdrdevice)

set (CMAKE_CXX_STANDARD 11)

set(soapysdrdevice_SOURCES
	devicesoapysdr.cpp
    devicesoapysdrscan.cpp
    devicesoapysdrshared.cpp
    devicesoapysdrparams.cpp
)

set(soapysdrdevice_HEADERS
	devicesoapysdr.h
    devicesoapysdrscan.h
    devicesoapysdrshared.h
    devicesoapysdrparams.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SOAPYSDR_SOURCE_DIR}/include
    ${SOAPYSDR_SOURCE_DIR}/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${SOAPYSDR_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#add_definitions(${QT_DEFINITIONS})
#add_definitions(-DQT_SHARED)

add_library(soapysdrdevice SHARED
    ${soapysdrdevice_SOURCES}
)

if (BUILD_DEBIAN)
target_link_libraries(soapysdrdevice
    soapysdr
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(soapysdrdevice
    ${SOAPYSDR_LIBRARY}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS soapysdrdevice DESTINATION lib)
