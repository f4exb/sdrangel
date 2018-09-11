project(udpsink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/udpsink")

set(udpsink_SOURCES
	${PLUGIN_PREFIX}/udpsink.cpp
	${PLUGIN_PREFIX}/udpsinkplugin.cpp
	${PLUGIN_PREFIX}/udpsinksettings.cpp
)

set(udpsink_HEADERS
	${PLUGIN_PREFIX}/udpsink.h
	${PLUGIN_PREFIX}/udpsinkplugin.h
	${PLUGIN_PREFIX}/udpsinksettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client			
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(udpsinksrv SHARED
	${udpsink_SOURCES}
	${udpsink_HEADERS_MOC}
)

target_link_libraries(udpsinksrv
	${QT_LIBRARIES}
	sdrbase
)

target_link_libraries(udpsinksrv Qt5::Core Qt5::Network)

install(TARGETS udpsinksrv DESTINATION lib/pluginssrv/channelrx)
