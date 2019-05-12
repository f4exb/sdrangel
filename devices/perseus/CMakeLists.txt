project(perseusdevice)

set (CMAKE_CXX_STANDARD 11)

set(perseusdevice_SOURCES
    deviceperseus.cpp
    deviceperseusscan.cpp
)

set(perseusdevice_HEADERS
    deviceperseus.h
    deviceperseusscan.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBPERSEUSSRC}
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBUSB_INCLUDE_DIR}
    ${LIBPERSEUS_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#add_definitions(${QT_DEFINITIONS})
#add_definitions(-DQT_SHARED)

add_library(perseusdevice SHARED
    ${perseusdevice_SOURCES}
)

set_target_properties(perseusdevice PROPERTIES DEFINE_SYMBOL "devices_EXPORTS")

if (BUILD_DEBIAN)
target_link_libraries(perseusdevice
    ${LIBUSB_LIBRARIES}
    perseus
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(perseusdevice
    ${LIBUSB_LIBRARIES}
    ${LIBPERSEUS_LIBRARIES}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS perseusdevice DESTINATION lib)
