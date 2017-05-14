project(sdrdaemondevice)

if (HAS_SSSE3)
    message(STATUS "SDRdaemonDevice: use SSSE3 SIMD" )
elseif (HAS_NEON)
    message(STATUS "SDRdaemonDevice: use Neon SIMD" )
else()
    message(STATUS "SDRdaemonDevice: Unsupported architecture")
    return()
endif()

set(sdrdaemondevice_SOURCES
    sdrdaemonfecbuffer.cpp
)

set(sdrdaemondevice_HEADERS
    sdrdaemonfecbuffer.h
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_SHARED)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${LIBCM256CCSRC}
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CM256CC_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_library(sdrdaemondevice SHARED
    ${sdrdaemondevice_SOURCES}
)

if (BUILD_DEBIAN)
target_link_libraries(sdrdaemondevice
    ${QT_LIBRARIES}    
    cm256cc
)
else (BUILD_DEBIAN)
target_link_libraries(sdrdaemondevice
    ${QT_LIBRARIES}
    ${CM256CC_LIBRARIES}
)
endif (BUILD_DEBIAN)

qt5_use_modules(sdrdaemondevice Core)

install(TARGETS sdrdaemondevice DESTINATION lib)
