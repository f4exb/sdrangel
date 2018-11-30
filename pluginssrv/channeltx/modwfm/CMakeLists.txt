project(modwfm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/modwfm")

set(modwfm_SOURCES
	${PLUGIN_PREFIX}/wfmmod.cpp
	${PLUGIN_PREFIX}/wfmmodplugin.cpp
	${PLUGIN_PREFIX}/wfmmodsettings.cpp
)

set(modwfm_HEADERS
	${PLUGIN_PREFIX}/wfmmod.h
	${PLUGIN_PREFIX}/wfmmodplugin.h
	${PLUGIN_PREFIX}/wfmmodsettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(modwfmsrv SHARED
	${modwfm_SOURCES}
	${modwfm_HEADERS_MOC}
)

target_link_libraries(modwfmsrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(modwfmsrv Qt5::Core)

install(TARGETS modwfmsrv DESTINATION lib/pluginssrv/channeltx)