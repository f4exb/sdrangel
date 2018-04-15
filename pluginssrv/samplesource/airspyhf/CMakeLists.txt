project(airspyhf)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/airspyhf")

set(airspyhf_SOURCES
    ${PLUGIN_PREFIX}/airspyhfinput.cpp
    ${PLUGIN_PREFIX}/airspyhfplugin.cpp
    ${PLUGIN_PREFIX}/airspyhfsettings.cpp
    ${PLUGIN_PREFIX}/airspyhfthread.cpp
)

set(airspyhf_HEADERS
    ${PLUGIN_PREFIX}/airspyhfinput.h
    ${PLUGIN_PREFIX}/airspyhfplugin.h
    ${PLUGIN_PREFIX}/airspyhfsettings.h
    ${PLUGIN_PREFIX}/airspyhfthread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBAIRSPYHFSRC}
    ${LIBAIRSPYHFSRC}/libairspyhf/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client        
    ${LIBAIRSPYHF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputairspyhfsrv SHARED
    ${airspyhf_SOURCES}
    ${airspyhf_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputairspyhfsrv
    ${QT_LIBRARIES}
    airspyhf
    sdrbase
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputairspyhfsrv
    ${QT_LIBRARIES}
    ${LIBAIRSPYHF_LIBRARIES}
    sdrbase
    swagger
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputairspyhfsrv Core)

install(TARGETS inputairspyhfsrv DESTINATION lib/pluginssrv/samplesource)
