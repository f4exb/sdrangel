project(modfreedv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/modfreedv")

set(modfreedv_SOURCES
    ${PLUGIN_PREFIX}/freedvmod.cpp
    ${PLUGIN_PREFIX}/freedvmodplugin.cpp
    ${PLUGIN_PREFIX}/freedvmodsettings.cpp
)

set(modfreedv_HEADERS
    ${PLUGIN_PREFIX}/freedvmod.h
    ${PLUGIN_PREFIX}/freedvmodplugin.h
    ${PLUGIN_PREFIX}/freedvmodsettings.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/libfreedv
    ${CODEC2_INCLUDE_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(modfreedvsrv SHARED
    ${modfreedv_SOURCES}
    ${modfreedv_HEADERS_MOC}
)

target_link_libraries(modfreedvsrv
    ${QT_LIBRARIES}
    sdrbase
    swagger
	freedv
    ${CODEC2_LIBRARIES}
)

target_link_libraries(modfreedvsrv Qt5::Core)

install(TARGETS modfreedvsrv DESTINATION lib/pluginssrv/channeltx)
