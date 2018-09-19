project(bladerf1device)

set(bladerf1device_SOURCES
    devicebladerf1.cpp
    devicebladerf1values.cpp
    devicebladerf1shared.cpp
)

set(bladerf1device_HEADERS
    devicebladerf1.h
    devicebladerf1values.h
    devicebladerf1param.h
    devicebladerf1shared.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBBLADERFLIBSRC}/include
    ${LIBBLADERFLIBSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBBLADERF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#add_definitions(${QT_DEFINITIONS})
#add_definitions(-DQT_SHARED)

add_library(bladerf1device SHARED
    ${bladerf1device_SOURCES}
)

if (BUILD_DEBIAN)
target_link_libraries(bladerf1device
    bladerf
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(bladerf1device
    ${LIBBLADERF_LIBRARIES}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS bladerf1device DESTINATION lib)
