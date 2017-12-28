project(hackrfoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/hackrfoutput")

set(hackrfoutput_SOURCES
    ${PLUGIN_PREFIX}/hackrfoutput.cpp
    ${PLUGIN_PREFIX}/hackrfoutputplugin.cpp
    ${PLUGIN_PREFIX}/hackrfoutputsettings.cpp
    ${PLUGIN_PREFIX}/hackrfoutputthread.cpp
)

set(hackrfoutput_HEADERS
    ${PLUGIN_PREFIX}/hackrfoutput.h
    ${PLUGIN_PREFIX}/hackrfoutputplugin.h
    ${PLUGIN_PREFIX}/hackrfoutputsettings.h
    ${PLUGIN_PREFIX}/hackrfoutputthread.h
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

add_library(outputhackrfsrv SHARED
    ${hackrfoutput_SOURCES}
    ${hackrfoutput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputhackrfsrv
    ${QT_LIBRARIES}
    hackrf
    sdrbase
    swagger
    hackrfdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputhackrfsrv
    ${QT_LIBRARIES}
    ${LIBHACKRF_LIBRARIES}
    sdrbase
    swagger
    hackrfdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(outputhackrfsrv Core)

install(TARGETS outputhackrfsrv DESTINATION lib/pluginssrv/samplesink)
