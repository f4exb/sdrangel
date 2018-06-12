project(fcdpro)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/fcdpro")

set(fcdpro_SOURCES
	${PLUGIN_PREFIX}/fcdproinput.cpp
	${PLUGIN_PREFIX}/fcdproplugin.cpp
	${PLUGIN_PREFIX}/fcdprosettings.cpp
	${PLUGIN_PREFIX}/fcdprothread.cpp
)

set(fcdpro_HEADERS
	${PLUGIN_PREFIX}/fcdproinput.h
	${PLUGIN_PREFIX}/fcdproplugin.h
	${PLUGIN_PREFIX}/fcdprosettings.h
	${PLUGIN_PREFIX}/fcdprothread.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
	${CMAKE_SOURCE_DIR}/fcdhid
	${CMAKE_SOURCE_DIR}/fcdlib
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputfcdprosrv SHARED
	${fcdpro_SOURCES}
	${fcdpro_HEADERS_MOC}
)

target_link_libraries(inputfcdprosrv
	${QT_LIBRARIES}
	asound
	fcdhid
	fcdlib
	sdrbase
	swagger
)

target_link_libraries(inputfcdprosrv Qt5::Core)

install(TARGETS inputfcdprosrv DESTINATION lib/pluginssrv/samplesource)