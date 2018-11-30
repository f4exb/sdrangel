project(modam)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/modam")

set(modam_SOURCES
	${PLUGIN_PREFIX}/ammod.cpp
	${PLUGIN_PREFIX}/ammodplugin.cpp
	${PLUGIN_PREFIX}/ammodsettings.cpp
)

set(modam_HEADERS
	${PLUGIN_PREFIX}/ammod.h
	${PLUGIN_PREFIX}/ammodplugin.h
	${PLUGIN_PREFIX}/ammodsettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(modamsrv SHARED
	${modam_SOURCES}
	${modam_HEADERS_MOC}
)

target_link_libraries(modamsrv
	${QT_LIBRARIES}
	sdrbase
	swagger
)

target_link_libraries(modamsrv Qt5::Core)

install(TARGETS modamsrv DESTINATION lib/pluginssrv/channeltx)