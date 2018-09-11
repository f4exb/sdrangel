project(daemonsink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/daemonsink")

set(daemonsink_SOURCES
	${PLUGIN_PREFIX}/daemonsink.cpp
	${PLUGIN_PREFIX}/daemonsinksettings.cpp
	${PLUGIN_PREFIX}/daemonsinkthread.cpp
	${PLUGIN_PREFIX}/daemonsinkplugin.cpp
)

set(daemonsink_HEADERS
	${PLUGIN_PREFIX}/daemonsink.h
	${PLUGIN_PREFIX}/daemonsinksettings.h
	${PLUGIN_PREFIX}/daemonsinkthread.h
	${PLUGIN_PREFIX}/daemonsinkplugin.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CM256CC_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(daemonsinksrv SHARED
	${daemonsink_SOURCES}
	${daemonsink_HEADERS_MOC}
)

target_link_libraries(daemonsinksrv
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(daemonsinksrv Qt5::Core)

install(TARGETS daemonsinksrv DESTINATION lib/pluginssrv/channelrx)
