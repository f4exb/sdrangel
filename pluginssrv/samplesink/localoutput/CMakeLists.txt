project(localoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/localoutput")

set(localoutput_SOURCES
	${PLUGIN_PREFIX}/localoutput.cpp
	${PLUGIN_PREFIX}/localoutputplugin.cpp
	${PLUGIN_PREFIX}/localoutputsettings.cpp
)

set(localoutput_HEADERS
	${PLUGIN_PREFIX}/localoutput.h
	${PLUGIN_PREFIX}/localoutputplugin.h
	${PLUGIN_PREFIX}/localoutputsettings.h
)

include_directories(
    .
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(outputlocalsrv SHARED
	${localoutput_SOURCES}
	${localoutput_HEADERS_MOC}
)

target_link_libraries(outputlocalsrv
    ${QT_LIBRARIES}
    sdrbase
    swagger
)

target_link_libraries(outputlocalsrv Qt5::Core)

install(TARGETS outputlocalsrv DESTINATION lib/pluginssrv/samplesink)
