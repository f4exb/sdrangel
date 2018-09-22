project(bladerf2device)

set(bladerf2device_SOURCES
    devicebladerf2.cpp
)

set(bladerf2device_HEADERS
    devicebladerf2.h
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

add_library(bladerf2device SHARED
    ${bladerf2device_SOURCES}
)

if (BUILD_DEBIAN)
target_link_libraries(bladerf2device
    bladerf
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(bladerf2device
    ${LIBBLADERF_LIBRARIES}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS bladerf2device DESTINATION lib)
