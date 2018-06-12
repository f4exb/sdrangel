project(bfm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/demodbfm")

set(bfm_SOURCES
	${PLUGIN_PREFIX}/bfmdemod.cpp
	${PLUGIN_PREFIX}/bfmdemodsettings.cpp
	${PLUGIN_PREFIX}/bfmplugin.cpp
	${PLUGIN_PREFIX}/rdsdemod.cpp
	${PLUGIN_PREFIX}/rdsdecoder.cpp
	${PLUGIN_PREFIX}/rdsparser.cpp
	${PLUGIN_PREFIX}/rdstmc.cpp
)

set(bfm_HEADERS
	${PLUGIN_PREFIX}/bfmdemod.h
	${PLUGIN_PREFIX}/bfmdemodsettings.h
	${PLUGIN_PREFIX}/bfmplugin.h
	${PLUGIN_PREFIX}/rdsdemod.h
	${PLUGIN_PREFIX}/rdsdecoder.h
	${PLUGIN_PREFIX}/rdsparser.h
	${PLUGIN_PREFIX}/rdstmc.h
)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(rdstmc.cpp PROPERTIES COMPILE_FLAGS -fno-var-tracking-assignments)
endif()

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(demodbfmsrv SHARED
	${bfm_SOURCES}
	${bfm_HEADERS_MOC}
	${bfm_FORMS_HEADERS}
)

target_link_libraries(demodbfmsrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(demodbfmsrv Qt5::Core)

install(TARGETS demodbfmsrv DESTINATION lib/pluginssrv/channelrx)
