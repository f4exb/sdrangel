project(ssb)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/demodssb")

set(ssb_SOURCES
	${PLUGIN_PREFIX}/ssbdemod.cpp
	${PLUGIN_PREFIX}/ssbdemodsettings.cpp
	${PLUGIN_PREFIX}/ssbplugin.cpp
)

set(ssb_HEADERS
	${PLUGIN_PREFIX}/ssbdemod.h
	${PLUGIN_PREFIX}/ssbdemodsettings.h
	${PLUGIN_PREFIX}/ssbplugin.h
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

#qt5_wrap_cpp(ssb_HEADERS_MOC ${ssb_HEADERS})

add_library(demodssbsrv SHARED
	${ssb_SOURCES}
	${ssb_HEADERS_MOC}
)

target_link_libraries(demodssbsrv
	${QT_LIBRARIES}
	sdrbase
)

target_link_libraries(demodssbsrv Qt5::Core)

install(TARGETS demodssbsrv DESTINATION lib/pluginssrv/channelrx)
