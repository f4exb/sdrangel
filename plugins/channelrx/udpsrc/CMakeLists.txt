project(udpsrc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(udpsrc_SOURCES
	udpsrc.cpp
	udpsrcgui.cpp
	udpsrcplugin.cpp
	udpsrcsettings.cpp
)

set(udpsrc_HEADERS
	udpsrc.h
	udpsrcgui.h
	udpsrcplugin.h
	udpsrcsettings.h
)

set(udpsrc_FORMS
	udpsrcgui.ui
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

qt5_wrap_ui(udpsrc_FORMS_HEADERS ${udpsrc_FORMS})

add_library(demodudpsrc SHARED
	${udpsrc_SOURCES}
	${udpsrc_HEADERS_MOC}
	${udpsrc_FORMS_HEADERS}
)

target_link_libraries(demodudpsrc
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

target_link_libraries(demodudpsrc Qt5::Core Qt5::Widgets Qt5::Network)

install(TARGETS demodudpsrc DESTINATION lib/plugins/channelrx)
