project(modssb)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/modssb")

set(modssb_SOURCES
    ${PLUGIN_PREFIX}/ssbmod.cpp
    ${PLUGIN_PREFIX}/ssbmodplugin.cpp
    ${PLUGIN_PREFIX}/ssbmodsettings.cpp
)

set(modssb_HEADERS
    ${PLUGIN_PREFIX}/ssbmod.h
    ${PLUGIN_PREFIX}/ssbmodplugin.h
    ${PLUGIN_PREFIX}/ssbmodsettings.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(modssbsrv SHARED
    ${modssb_SOURCES}
    ${modssb_HEADERS_MOC}
)

target_link_libraries(modssbsrv
    ${QT_LIBRARIES}
    sdrbase
	swagger    
)

target_link_libraries(modssbsrv Qt5::Core)

install(TARGETS modssbsrv DESTINATION lib/pluginssrv/channeltx)