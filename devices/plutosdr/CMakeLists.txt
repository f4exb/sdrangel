project(plutosdrdevice)

set (CMAKE_CXX_STANDARD 11)

set(plutosdrdevice_SOURCES
    deviceplutosdr.cpp
    deviceplutosdrbox.cpp
    deviceplutosdrscan.cpp
    deviceplutosdrparams.cpp
    deviceplutosdrshared.cpp
)

set(plutosdrdevice_HEADERS
    deviceplutosdr.h
    deviceplutsdrobox.h
    deviceplutosdrscan.h
    deviceplutosdrparams.h
    deviceplutosdrshared.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBIIOSRC}
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBIIO_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#add_definitions(${QT_DEFINITIONS})
#add_definitions(-DQT_SHARED)

add_library(plutosdrdevice SHARED
    ${plutosdrdevice_SOURCES}
)

if (BUILD_DEBIAN)
target_link_libraries(plutosdrdevice
    libiio
    sdrbase
)
else (BUILD_DEBIAN)
target_link_libraries(plutosdrdevice
    ${LIBIIO_LIBRARIES}
    sdrbase
)
endif (BUILD_DEBIAN)

install(TARGETS plutosdrdevice DESTINATION lib)
