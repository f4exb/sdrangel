project(limesdrdevice)

set(limesdrdevice_SOURCES
    devicelimesdrparam.cpp
)

set(limesdrdevice_HEADERS
    devicelimesdrparam.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIMESUITESRC}/include
    ${LIMESUITESRC}/src
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
