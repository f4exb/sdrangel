project(daemonsrc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/daemonsrc")

set(daemonsrc_SOURCES
	${PLUGIN_PREFIX}/daemonsrc.cpp
	${PLUGIN_PREFIX}/daemonsrcthread.cpp
	${PLUGIN_PREFIX}/daemonsrcplugin.cpp
	${PLUGIN_PREFIX}/daemonsrcsettings.cpp
)

set(daemonsrc_HEADERS
	${PLUGIN_PREFIX}/daemonsrc.h
	${PLUGIN_PREFIX}/daemonsrcthread.h
	${PLUGIN_PREFIX}/daemonsrcplugin.h
	${PLUGIN_PREFIX}/daemonsrcsettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/sdrdaemon
	${CM256CC_INCLUDE_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(daemonsrcsrv SHARED
	${daemonsrc_SOURCES}
	${daemonsrc_HEADERS_MOC}
)

target_link_libraries(daemonsrcsrv
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	sdrdaemon
	swagger
)

target_link_libraries(daemonsrcsrv Qt5::Core Qt5::Network)

install(TARGETS daemonsrcsrv DESTINATION lib/pluginssrv/channeltx)
