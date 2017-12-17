project(nfm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/demodnfm")

set(nfm_SOURCES
	${PLUGIN_PREFIX}/nfmdemod.cpp
	${PLUGIN_PREFIX}/nfmdemodsettings.cpp
	${PLUGIN_PREFIX}/nfmplugin.cpp
)

set(nfm_HEADERS
	${PLUGIN_PREFIX}/nfmdemod.h
	${PLUGIN_PREFIX}/nfmdemodsettings.h
	${PLUGIN_PREFIX}/nfmplugin.h
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

add_library(demodnfmsrv SHARED
	${nfm_SOURCES}
	${nfm_HEADERS_MOC}
	${nfm_FORMS_HEADERS}
)

target_link_libraries(demodnfmsrv
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

qt5_use_modules(demodnfmsrv Core)

install(TARGETS demodnfmsrv DESTINATION lib/pluginssrv/channelrx)