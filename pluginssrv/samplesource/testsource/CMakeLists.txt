project(testsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/testsource")

set(testsource_SOURCES
	${PLUGIN_PREFIX}/testsourceinput.cpp
	${PLUGIN_PREFIX}/testsourceplugin.cpp
	${PLUGIN_PREFIX}/testsourcethread.cpp
	${PLUGIN_PREFIX}/testsourcesettings.cpp
)

set(testsource_HEADERS
	${PLUGIN_PREFIX}/testsourceinput.h
	${PLUGIN_PREFIX}/testsourceplugin.h
	${PLUGIN_PREFIX}/testsourcethread.h
	${PLUGIN_PREFIX}/testsourcesettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputtestsourcesrv SHARED
	${testsource_SOURCES}
	${testsource_HEADERS_MOC}
)

target_link_libraries(inputtestsourcesrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(inputtestsourcesrv Qt5::Core)

install(TARGETS inputtestsourcesrv DESTINATION lib/pluginssrv/samplesource)
