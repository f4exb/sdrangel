project(bladerfdevice)

set(bladerfdevice_SOURCES
    devicebladerf.cpp
    devicebladerfvalues.cpp
    devicebladerfshared.cpp
)

set(bladerfdevice_HEADERS
    devicebladerf.h
    devicebladerfvalues.h
    devicebladerfparam.h
    devicebladerfshared.h
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

add_library(bladerfdevice SHARED
    ${bladerfdevice_SOURCES}
)

if (BUILD_DEBIAN)
target_link_libraries(bladerfdevice
    bladerf
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(bladerfdevice
    ${LIBBLADERF_LIBRARIES}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS bladerfdevice DESTINATION lib)
