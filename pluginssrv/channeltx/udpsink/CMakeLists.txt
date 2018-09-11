project(udpsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/udpsink")

set(udpsource_SOURCES
	${PLUGIN_PREFIX}/udpsource.cpp
	${PLUGIN_PREFIX}/udpsourceplugin.cpp
	${PLUGIN_PREFIX}/udpsourceudphandler.cpp
	${PLUGIN_PREFIX}/udpsourcemsg.cpp
	${PLUGIN_PREFIX}/udpsourcesettings.cpp
)

set(udpsource_HEADERS
	${PLUGIN_PREFIX}/udpsource.h
	${PLUGIN_PREFIX}/udpsourceplugin.h
	${PLUGIN_PREFIX}/udpsourceudphandler.h
	${PLUGIN_PREFIX}/udpsourcemsg.h
	${PLUGIN_PREFIX}/udpsourcesettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(udpsourcesrv SHARED
	${udpsource_SOURCES}
	${udpsource_HEADERS_MOC}
)

target_link_libraries(udpsourcesrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(udpsourcesrv Qt5::Core Qt5::Network)

install(TARGETS udpsourcesrv DESTINATION lib/pluginssrv/channeltx)
