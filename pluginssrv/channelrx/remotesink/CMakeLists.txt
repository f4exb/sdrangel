project(remotesink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/remotesink")

set(remotesink_SOURCES
	${PLUGIN_PREFIX}/remotesink.cpp
	${PLUGIN_PREFIX}/remotesinksettings.cpp
	${PLUGIN_PREFIX}/remotesinkthread.cpp
	${PLUGIN_PREFIX}/remotesinkplugin.cpp
)

set(remotesink_HEADERS
	${PLUGIN_PREFIX}/remotesink.h
	${PLUGIN_PREFIX}/remotesinksettings.h
	${PLUGIN_PREFIX}/remotesinkthread.h
	${PLUGIN_PREFIX}/remotesinkplugin.h
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

add_library(remotesinksrv SHARED
	${remotesink_SOURCES}
	${remotesink_HEADERS_MOC}
)

target_link_libraries(remotesinksrv
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(remotesinksrv Qt5::Core)

install(TARGETS remotesinksrv DESTINATION lib/pluginssrv/channelrx)
