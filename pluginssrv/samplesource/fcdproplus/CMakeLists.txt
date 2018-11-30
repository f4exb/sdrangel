project(fcdproplus)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/fcdproplus")

set(fcdproplus_SOURCES
	${PLUGIN_PREFIX}/fcdproplusinput.cpp
	${PLUGIN_PREFIX}/fcdproplusplugin.cpp
	${PLUGIN_PREFIX}/fcdproplussettings.cpp
	${PLUGIN_PREFIX}/fcdproplusthread.cpp
)

set(fcdproplus_HEADERS
	${PLUGIN_PREFIX}/fcdproplusinput.h
	${PLUGIN_PREFIX}/fcdproplusplugin.h
	${PLUGIN_PREFIX}/fcdproplussettings.h
	${PLUGIN_PREFIX}/fcdproplusthread.h
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

add_library(inputfcdproplussrv SHARED
	${fcdproplus_SOURCES}
	${fcdproplus_HEADERS_MOC}
)

target_link_libraries(inputfcdproplussrv
	${QT_LIBRARIES}
	asound
	fcdhid
	fcdlib
	sdrbase
	swagger
)

target_link_libraries(inputfcdproplussrv Qt5::Core)

install(TARGETS inputfcdproplussrv DESTINATION lib/pluginssrv/samplesource)
