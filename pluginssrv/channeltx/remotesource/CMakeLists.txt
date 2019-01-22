project(remotesource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/remotesource")

set(remotesource_SOURCES
	${PLUGIN_PREFIX}/remotesource.cpp
	${PLUGIN_PREFIX}/remotesourcethread.cpp
	${PLUGIN_PREFIX}/remotesourceplugin.cpp
	${PLUGIN_PREFIX}/remotesourcesettings.cpp
)

set(remotesource_HEADERS
	${PLUGIN_PREFIX}/remotesource.h
	${PLUGIN_PREFIX}/remotesourcethread.h
	${PLUGIN_PREFIX}/remotesourceplugin.h
	${PLUGIN_PREFIX}/remotesourcesettings.h
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

add_library(remotesourcesrv SHARED
	${remotesource_SOURCES}
	${remotesource_HEADERS_MOC}
)

target_link_libraries(remotesourcesrv
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(remotesourcesrv Qt5::Core Qt5::Network)

install(TARGETS remotesourcesrv DESTINATION lib/pluginssrv/channeltx)
