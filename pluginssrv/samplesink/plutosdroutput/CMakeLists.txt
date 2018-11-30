project(plutosdroutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/plutosdroutput")

set(plutosdroutput_SOURCES
    ${PLUGIN_PREFIX}/plutosdroutput.cpp
    ${PLUGIN_PREFIX}/plutosdroutputplugin.cpp
    ${PLUGIN_PREFIX}/plutosdroutputsettings.cpp
    ${PLUGIN_PREFIX}/plutosdroutputthread.cpp
)

set(plutosdroutput_HEADERS
    ${PLUGIN_PREFIX}/plutosdroutput.h
    ${PLUGIN_PREFIX}/plutosdroutputplugin.h
    ${PLUGIN_PREFIX}/plutosdroutputsettings.h
    ${PLUGIN_PREFIX}/plutosdroutputthread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBIIOSRC}
)
else (BUILD_DEBIAN)
include_directories(
        .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
    ${CMAKE_SOURCE_DIR}/devices    
    ${LIBIIO_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(outputplutosdrsrv SHARED
    ${plutosdroutput_SOURCES}
    ${plutosdroutput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(outputplutosdrsrv
    ${QT_LIBRARIES}
    iio
    sdrbase
    swagger
    plutosdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(outputplutosdrsrv
    ${QT_LIBRARIES}
    ${LIBIIO_LIBRARIES}
    sdrbase
    swagger
    plutosdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(outputplutosdrsrv Core)

install(TARGETS outputplutosdrsrv DESTINATION lib/pluginssrv/samplesink)
