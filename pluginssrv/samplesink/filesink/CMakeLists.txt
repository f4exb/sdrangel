project(filesink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/samplesink/filesink")

set(filesink_SOURCES
	${PLUGIN_PREFIX}/filesinkoutput.cpp
	${PLUGIN_PREFIX}/filesinkplugin.cpp
	${PLUGIN_PREFIX}/filesinksettings.cpp
	${PLUGIN_PREFIX}/filesinkthread.cpp
)

set(filesink_HEADERS
	${PLUGIN_PREFIX}/filesinkoutput.h
	${PLUGIN_PREFIX}/filesinkplugin.h
	${PLUGIN_PREFIX}/filesinksettings.h
	${PLUGIN_PREFIX}/filesinkthread.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client    
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(outputfilesinksrv SHARED
	${filesink_SOURCES}
	${filesink_HEADERS_MOC}
)

target_link_libraries(outputfilesinksrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

qt5_use_modules(outputfilesinksrv Core)

install(TARGETS outputfilesinksrv DESTINATION lib/pluginssrv/samplesink)
