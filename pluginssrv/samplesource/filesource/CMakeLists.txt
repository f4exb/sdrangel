project(filesource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/filesource")

set(filesource_SOURCES
	${PLUGIN_PREFIX}/filesourceinput.cpp
	${PLUGIN_PREFIX}/filesourceplugin.cpp
	${PLUGIN_PREFIX}/filesourcethread.cpp
	${PLUGIN_PREFIX}/filesourcesettings.cpp
)

set(filesource_HEADERS
	${PLUGIN_PREFIX}/filesourceinput.h
	${PLUGIN_PREFIX}/filesourceplugin.h
	${PLUGIN_PREFIX}/filesourcethread.h
    ${PLUGIN_PREFIX}/filesourcesettings.h
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

add_library(inputfilesourcesrv SHARED
	${filesource_SOURCES}
	${filesource_HEADERS_MOC}
)

target_link_libraries(inputfilesourcesrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

qt5_use_modules(inputfilesourcesrv Core)

install(TARGETS inputfilesourcesrv DESTINATION lib/pluginssrv/samplesource)
