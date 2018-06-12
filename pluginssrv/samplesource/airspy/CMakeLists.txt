project(airspy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/airspy")

set(airspy_SOURCES
    ${PLUGIN_PREFIX}/airspyinput.cpp
    ${PLUGIN_PREFIX}/airspyplugin.cpp
    ${PLUGIN_PREFIX}/airspysettings.cpp
    ${PLUGIN_PREFIX}/airspythread.cpp
)

set(airspy_HEADERS
    ${PLUGIN_PREFIX}/airspyinput.h
    ${PLUGIN_PREFIX}/airspyplugin.h
    ${PLUGIN_PREFIX}/airspysettings.h
    ${PLUGIN_PREFIX}/airspythread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBAIRSPYSRC}
    ${LIBAIRSPYSRC}/libairspy/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBAIRSPY_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputairspysrv SHARED
    ${airspy_SOURCES}
    ${airspy_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputairspysrv
    ${QT_LIBRARIES}
    airspy
    sdrbase
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputairspysrv
    ${QT_LIBRARIES}
    ${LIBAIRSPY_LIBRARIES}
    sdrbase
    swagger
)
endif (BUILD_DEBIAN)


target_link_libraries(inputairspysrv Qt5::Core)

install(TARGETS inputairspysrv DESTINATION lib/pluginssrv/samplesource)
