project(xtrxoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/xtrxoutput")

set(xtrxoutput_SOURCES
	${PLUGIN_PREFIX}/xtrxoutput.cpp
	${PLUGIN_PREFIX}/xtrxoutputplugin.cpp
	${PLUGIN_PREFIX}/xtrxoutputsettings.cpp
	${PLUGIN_PREFIX}/xtrxoutputthread.cpp
)

set(xtrxoutput_HEADERS
	${PLUGIN_PREFIX}/xtrxoutput.h
	${PLUGIN_PREFIX}/xtrxoutputplugin.h
	${PLUGIN_PREFIX}/xtrxoutputsettings.h
	${PLUGIN_PREFIX}/xtrxoutputthread.h
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

add_library(outputxtrxsrv SHARED
	${xtrxoutput_SOURCES}
	${xtrxoutput_HEADERS_MOC}
)

target_link_libraries(outputxtrxsrv
	${QT_LIBRARIES}
    ${LIBXTRX_LIBRARY}
	sdrbase
	swagger
	xtrxdevice
)

target_link_libraries(outputxtrxsrv Qt5::Core)

install(TARGETS outputxtrxsrv DESTINATION lib/pluginssrv/samplesink)
