project(plutosdrinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/plutosdrinput")

set(plutosdrinput_SOURCES
    ${PLUGIN_PREFIX}/plutosdrinput.cpp
    ${PLUGIN_PREFIX}/plutosdrinputplugin.cpp
    ${PLUGIN_PREFIX}/plutosdrinputsettings.cpp
    ${PLUGIN_PREFIX}/plutosdrinputthread.cpp
)

set(plutosdrinput_HEADERS
    ${PLUGIN_PREFIX}/plutosdrinput.h
    ${PLUGIN_PREFIX}/plutosdrinputplugin.h
    ${PLUGIN_PREFIX}/plutosdrinputsettings.h
    ${PLUGIN_PREFIX}/plutosdrinputthread.h
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

add_library(inputplutosdrsrv SHARED
    ${plutosdrinput_SOURCES}
    ${plutosdrinput_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputplutosdrsrv
    ${QT_LIBRARIES}
    iio
    sdrbase
    swagger
    plutosdrdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputplutosdrsrv
    ${QT_LIBRARIES}
    ${LIBIIO_LIBRARIES}
    sdrbase
    swagger
    plutosdrdevice
)
endif (BUILD_DEBIAN)

qt5_use_modules(inputplutosdrsrv Core)

install(TARGETS inputplutosdrsrv DESTINATION lib/plugins/samplesource)
