project(rtlsdr)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/rtlsdr")

set(rtlsdr_SOURCES
    ${PLUGIN_PREFIX}/rtlsdrinput.cpp
    ${PLUGIN_PREFIX}/rtlsdrplugin.cpp
    ${PLUGIN_PREFIX}/rtlsdrsettings.cpp
    ${PLUGIN_PREFIX}/rtlsdrthread.cpp
)

set(rtlsdr_HEADERS
    ${PLUGIN_PREFIX}/rtlsdrinput.h
    ${PLUGIN_PREFIX}/rtlsdrplugin.h
    ${PLUGIN_PREFIX}/rtlsdrsettings.h
    ${PLUGIN_PREFIX}/rtlsdrthread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBRTLSDRSRC}/include
    ${LIBRTLSDRSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBRTLSDR_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputrtlsdrsrv SHARED
    ${rtlsdr_SOURCES}
    ${rtlsdr_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputrtlsdrsrv
    ${QT_LIBRARIES}
    rtlsdr
    sdrbase
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputrtlsdrsrv
    ${QT_LIBRARIES}
    ${LIBRTLSDR_LIBRARIES}
    sdrbase
    swagger
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputrtlsdrsrv Core)

install(TARGETS inputrtlsdrsrv DESTINATION lib/pluginssrv/samplesource)
