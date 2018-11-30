project(wfm)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(PLUGIN_PREFIX "../../../plugins/channelrx/demodwfm")

set(wfm_SOURCES
	${PLUGIN_PREFIX}/wfmdemod.cpp
	${PLUGIN_PREFIX}/wfmdemodsettings.cpp
	${PLUGIN_PREFIX}/wfmplugin.cpp
)

set(wfm_HEADERS
	${PLUGIN_PREFIX}/wfmdemod.h
	${PLUGIN_PREFIX}/wfmdemodsettings.h
	${PLUGIN_PREFIX}/wfmplugin.h
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

#qt5_wrap_cpp(nfm_HEADERS_MOC ${nfm_HEADERS})

add_library(demodwfmsrv SHARED
	${wfm_SOURCES}
	${wfm_HEADERS_MOC}
)

target_link_libraries(demodwfmsrv
	${QT_LIBRARIES}
	sdrbase
)

target_link_libraries(demodwfmsrv Qt5::Core)

install(TARGETS demodwfmsrv DESTINATION lib/pluginssrv/channelrx)