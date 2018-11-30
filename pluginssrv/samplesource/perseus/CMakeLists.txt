project(perseus)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/perseus")

set(perseus_SOURCES
    ${PLUGIN_PREFIX}/perseusinput.cpp
    ${PLUGIN_PREFIX}/perseusplugin.cpp
    ${PLUGIN_PREFIX}/perseussettings.cpp
    ${PLUGIN_PREFIX}/perseusthread.cpp
)

set(perseus_HEADERS
    ${PLUGIN_PREFIX}/perseusinput.h
    ${PLUGIN_PREFIX}/perseusplugin.h
    ${PLUGIN_PREFIX}/perseussettings.h
    ${PLUGIN_PREFIX}/perseusthread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices        
    ${LIBPERSEUSSRC}
    ${LIBPERSEUSSRC}/libperseus/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices        
    ${LIBPERSEUS_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions("${QT_DEFINITIONS} -DLIBAIRSPY_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputperseussrv SHARED
    ${perseus_SOURCES}
    ${perseus_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputperseussrv
    ${QT_LIBRARIES}
    perseus
    sdrbase
    swagger
    perseusdevice
)
else (BUILD_DEBIAN)
target_link_libraries(inputperseussrv
    ${QT_LIBRARIES}
    ${LIBPERSEUS_LIBRARIES}
    sdrbase
    swagger
    perseusdevice
)
endif (BUILD_DEBIAN)


qt5_use_modules(inputperseussrv Core)

install(TARGETS inputperseussrv DESTINATION lib/pluginssrv/samplesource)
