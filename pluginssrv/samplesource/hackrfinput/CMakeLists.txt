project(hackrfinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/hackrfinput")

set(hackrfinput_SOURCES
    ${PLUGIN_PREFIX}/hackrfinput.cpp
    ${PLUGIN_PREFIX}/hackrfinputplugin.cpp
    ${PLUGIN_PREFIX}/hackrfinputsettings.cpp
    ${PLUGIN_PREFIX}/hackrfinputthread.cpp
)

set(hackrfinput_HEADERS
    ${PLUGIN_PREFIX}/hackrfinput.h
    ${PLUGIN_PREFIX}/hackrfinputplugin.h
    ${PLUGIN_PREFIX}/hackrfinputsettings.h
    ${PLUGIN_PREFIX}/hackrfinputthread.h
)

if (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBHACKRFSRC}
    ${LIBHACKRFSRC}/libhackrf/src
)
else (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBHACKRF_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions("${QT_DEFINITIONS} -DLIBHACKRF_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputhackrfsrv SHARED
    ${hackrfinput_SOURCES}
    ${hackrfinput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputhackrfsrv
    ${QT_LIBRARIES}
    hackrf
    sdrbase
    swagger
    hackrfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputhackrfsrv
    ${QT_LIBRARIES}
    ${LIBHACKRF_LIBRARIES}
    sdrbase
    swagger
    hackrfdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputhackrfsrv Core)

install(TARGETS inputhackrfsrv DESTINATION lib/pluginssrv/samplesource)
