project(daemonsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/daemonsource")

set(daemonsource_SOURCES
	${PLUGIN_PREFIX}/daemonsource.cpp
	${PLUGIN_PREFIX}/daemonsourcethread.cpp
	${PLUGIN_PREFIX}/daemonsourceplugin.cpp
	${PLUGIN_PREFIX}/daemonsourcesettings.cpp
)

set(daemonsource_HEADERS
	${PLUGIN_PREFIX}/daemonsource.h
	${PLUGIN_PREFIX}/daemonsourcethread.h
	${PLUGIN_PREFIX}/daemonsourceplugin.h
	${PLUGIN_PREFIX}/daemonsourcesettings.h
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

add_library(daemonsourcesrv SHARED
	${daemonsource_SOURCES}
	${daemonsource_HEADERS_MOC}
)

target_link_libraries(daemonsourcesrv
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(daemonsourcesrv Qt5::Core Qt5::Network)

install(TARGETS daemonsourcesrv DESTINATION lib/pluginssrv/channeltx)
