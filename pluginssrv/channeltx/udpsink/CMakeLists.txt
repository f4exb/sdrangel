project(udpsink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/udpsink")

set(udpsink_SOURCES
	${PLUGIN_PREFIX}/udpsink.cpp
	${PLUGIN_PREFIX}/udpsinkplugin.cpp
	${PLUGIN_PREFIX}/udpsinkudphandler.cpp
	${PLUGIN_PREFIX}/udpsinkmsg.cpp
	${PLUGIN_PREFIX}/udpsinksettings.cpp
)

set(udpsink_HEADERS
	${PLUGIN_PREFIX}/udpsink.h
	${PLUGIN_PREFIX}/udpsinkplugin.h
	${PLUGIN_PREFIX}/udpsinkudphandler.h
	${PLUGIN_PREFIX}/udpsinkmsg.h
	${PLUGIN_PREFIX}/udpsinksettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(modudpsinksrv SHARED
	${udpsink_SOURCES}
	${udpsink_HEADERS_MOC}
)

target_link_libraries(modudpsinksrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(modudpsinksrv Qt5::Core Qt5::Network)

install(TARGETS modudpsinksrv DESTINATION lib/pluginssrv/channeltx)
