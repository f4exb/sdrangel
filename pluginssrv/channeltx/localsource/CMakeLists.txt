project(localsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/localsource")

set(localsource_SOURCES
	${PLUGIN_PREFIX}/localsource.cpp
	${PLUGIN_PREFIX}/localsourcethread.cpp
	${PLUGIN_PREFIX}/localsourceplugin.cpp
	${PLUGIN_PREFIX}/localsourcesettings.cpp
)

set(localsource_HEADERS
	${PLUGIN_PREFIX}/localsource.h
	${PLUGIN_PREFIX}/localsourcethread.h
	${PLUGIN_PREFIX}/localsourceplugin.h
	${PLUGIN_PREFIX}/localsourcesettings.h
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

add_library(localsourcesrv SHARED
	${localsource_SOURCES}
	${localsource_HEADERS_MOC}
)

target_link_libraries(localsourcesrv
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(localsourcesrv Qt5::Core Qt5::Network)

install(TARGETS localsourcesrv DESTINATION lib/pluginssrv/channeltx)
