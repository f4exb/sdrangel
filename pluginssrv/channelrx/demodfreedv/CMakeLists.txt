project(demodfreedv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/demodfreedv")

set(freedv_SOURCES
	${PLUGIN_PREFIX}/freedvdemod.cpp
	${PLUGIN_PREFIX}/freedvdemodsettings.cpp
	${PLUGIN_PREFIX}/freedvplugin.cpp
)

set(freedv_HEADERS
	${PLUGIN_PREFIX}/freedvdemod.h
	${PLUGIN_PREFIX}/freedvdemodsettings.h
	${PLUGIN_PREFIX}/freedvplugin.h
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

add_library(demodfreedvsrv SHARED
	${freedv_SOURCES}
	${freedv_HEADERS_MOC}
)

target_link_libraries(demodfreedvsrv
	${QT_LIBRARIES}
	sdrbase
	swagger
	freedv
	${CODEC2_LIBRARIES}
)

target_link_libraries(demodfreedvsrv Qt5::Core)

install(TARGETS demodfreedvsrv DESTINATION lib/pluginssrv/channelrx)
