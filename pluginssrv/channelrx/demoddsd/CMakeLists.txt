project(dsddemod)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/demoddsd")

set(dsddemod_SOURCES
    ${PLUGIN_PREFIX}/dsddemod.cpp
    ${PLUGIN_PREFIX}/dsddemodplugin.cpp
    ${PLUGIN_PREFIX}/dsddemodbaudrates.cpp
    ${PLUGIN_PREFIX}/dsddemodsettings.cpp
    ${PLUGIN_PREFIX}/dsddecoder.cpp
)

set(dsddemod_HEADERS
    ${PLUGIN_PREFIX}/dsddemod.h
    ${PLUGIN_PREFIX}/dsddemodplugin.h
    ${PLUGIN_PREFIX}/dsddemodbaudrates.h
    ${PLUGIN_PREFIX}/dsddemodsettings.h
    ${PLUGIN_PREFIX}/dsddecoder.h
)

if (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
    ${LIBDSDCCSRC}
    ${LIBMBELIBSRC}
)
else (BUILD_DEBIAN)
include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
    ${LIBDSDCC_INCLUDE_DIR}
    ${LIBMBE_INCLUDE_DIR}
)
endif (BUILD_DEBIAN)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(demoddsdsrv SHARED
    ${dsddemod_SOURCES}
    ${dsddemod_HEADERS_MOC}
)

if (BUILD_DEBIAN)
target_link_libraries(demoddsdsrv
    ${QT_LIBRARIES}
    sdrbase
    dsdcc
    mbelib
)
else (BUILD_DEBIAN)
target_link_libraries(demoddsdsrv
    ${QT_LIBRARIES}
    sdrbase
    ${LIBDSDCC_LIBRARIES}
    ${LIBMBE_LIBRARY}
)
endif (BUILD_DEBIAN)


target_link_libraries(demoddsdsrv Qt5::Core)

install(TARGETS demoddsdsrv DESTINATION lib/pluginssrv/channelrx)
