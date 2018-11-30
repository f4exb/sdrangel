project(am)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/demodam")

set(am_SOURCES
	${PLUGIN_PREFIX}/amdemod.cpp
	${PLUGIN_PREFIX}/amdemodsettings.cpp
	${PLUGIN_PREFIX}/amdemodplugin.cpp
)

set(am_HEADERS
	${PLUGIN_PREFIX}/amdemod.h
	${PLUGIN_PREFIX}/amdemodsettings.h
	${PLUGIN_PREFIX}/amdemodplugin.h
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

add_library(demodamsrv SHARED
	${am_SOURCES}
	${am_HEADERS_MOC}
)

target_link_libraries(demodamsrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(demodamsrv Qt5::Core)

install(TARGETS demodamsrv DESTINATION lib/pluginssrv/channelrx)