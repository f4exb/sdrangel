project(xtrxinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesource/xtrxinput")

set(xtrxinput_SOURCES
	${PLUGIN_PREFIX}/xtrxinput.cpp
	${PLUGIN_PREFIX}/xtrxinputplugin.cpp
	${PLUGIN_PREFIX}/xtrxinputsettings.cpp
	${PLUGIN_PREFIX}/xtrxinputthread.cpp
)

set(xtrxinput_HEADERS
	${PLUGIN_PREFIX}/xtrxinput.h
	${PLUGIN_PREFIX}/xtrxinputplugin.h
	${PLUGIN_PREFIX}/xtrxinputsettings.h
	${PLUGIN_PREFIX}/xtrxinputthread.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBXTRX_INCLUDE_DIRS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(inputxtrxsrv SHARED
	${xtrxinput_SOURCES}
	${xtrxinput_HEADERS_MOC}
)


target_link_libraries(inputxtrxsrv
	${QT_LIBRARIES}
	${LIBXTRX_LIBRARY}
	sdrbase
	swagger
	xtrxdevice
)

qt5_use_modules(inputxtrxsrv Core)

install(TARGETS inputxtrxsrv DESTINATION lib/pluginssrv/samplesource)