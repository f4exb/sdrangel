project(udpsrc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/udpsrc")

set(udpsrc_SOURCES
	${PLUGIN_PREFIX}/udpsrc.cpp
	${PLUGIN_PREFIX}/udpsrcplugin.cpp
	${PLUGIN_PREFIX}/udpsrcsettings.cpp
)

set(udpsrc_HEADERS
	${PLUGIN_PREFIX}/udpsrc.h
	${PLUGIN_PREFIX}/udpsrcplugin.h
	${PLUGIN_PREFIX}/udpsrcsettings.h
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

add_library(demodudpsrcsrv SHARED
	${udpsrc_SOURCES}
	${udpsrc_HEADERS_MOC}
)

target_link_libraries(demodudpsrcsrv
	${QT_LIBRARIES}
	sdrbase
)

target_link_libraries(demodudpsrcsrv Qt5::Core Qt5::Network)

install(TARGETS demodudpsrcsrv DESTINATION lib/pluginssrv/channelrx)
