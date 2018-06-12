project(sdrplay)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/sdrplay")

set(sdrplay_SOURCES
    ${PLUGIN_PREFIX}/sdrplayinput.cpp
    ${PLUGIN_PREFIX}/sdrplayplugin.cpp
    ${PLUGIN_PREFIX}/sdrplaysettings.cpp
    ${PLUGIN_PREFIX}/sdrplaythread.cpp
)

set(sdrplay_HEADERS
    ${PLUGIN_PREFIX}/sdrplayinput.h
    ${PLUGIN_PREFIX}/sdrplayplugin.h
    ${PLUGIN_PREFIX}/sdrplaysettings.h
    ${PLUGIN_PREFIX}/sdrplaythread.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBMIRISDRSRC}/include
    ${LIBMIRISDRSRC}/src
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBMIRISDR_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputsdrplaysrv SHARED
    ${sdrplay_SOURCES}
    ${sdrplay_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(inputsdrplaysrv
    ${QT_LIBRARIES}
    mirisdr
    sdrbase
    swagger
)
else (BUILD_DEBIAN)
target_link_libraries(inputsdrplaysrv
    ${QT_LIBRARIES}
    ${LIBMIRISDR_LIBRARIES}
    sdrbase
    swagger
)
endif (BUILD_DEBIAN)

target_link_libraries(inputsdrplaysrv Qt5::Core)

install(TARGETS inputsdrplaysrv DESTINATION lib/pluginssrv/samplesource)
