project(modnfm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channeltx/modnfm")

set(modnfm_SOURCES
	${PLUGIN_PREFIX}/nfmmod.cpp
	${PLUGIN_PREFIX}/nfmmodplugin.cpp
	${PLUGIN_PREFIX}/nfmmodsettings.cpp
)

set(modnfm_HEADERS
	${PLUGIN_PREFIX}/nfmmod.h
	${PLUGIN_PREFIX}/nfmmodplugin.h
	${PLUGIN_PREFIX}/nfmmodsettings.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(modnfmsrv SHARED
	${modnfm_SOURCES}
	${modnfm_HEADERS_MOC}
	${modnfm_FORMS_HEADERS}
)

target_link_libraries(modnfmsrv
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

qt5_use_modules(modnfmsrv Core)

install(TARGETS modnfmsrv DESTINATION lib/pluginssrv/channeltx)