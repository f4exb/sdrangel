project(freqtracker)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/freqtracker")

set(freqtracker_SOURCES
    ${PLUGIN_PREFIX}/freqtracker.cpp
	${PLUGIN_PREFIX}/freqtrackersettings.cpp
	${PLUGIN_PREFIX}/freqtrackerplugin.cpp
)

set(freqtracker_HEADERS
    ${PLUGIN_PREFIX}/freqtracker.h
	${PLUGIN_PREFIX}/freqtrackersettings.h
	${PLUGIN_PREFIX}/freqtrackerplugin.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(freqtrackersrv SHARED
	${freqtracker_SOURCES}
	${freqtracker_HEADERS_MOC}
)

target_link_libraries(freqtrackersrv
	${QT_LIBRARIES}
	sdrbase
)

target_link_libraries(freqtrackersrv Qt5::Core)

install(TARGETS freqtrackersrv DESTINATION lib/plugins/channelrx)
