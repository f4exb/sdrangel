project(tcpsrc)

set(tcpsrc_SOURCES
	tcpsrc.cpp
	tcpsrcgui.cpp
	tcpsrcplugin.cpp
	tcpsrcsettings.cpp
)

set(tcpsrc_HEADERS
	tcpsrc.h
	tcpsrcgui.h
	tcpsrcplugin.h
	tcpsrcsettings.h
)

set(tcpsrc_FORMS
	tcpsrcgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(tcpsrc_HEADERS_MOC ${tcpsrc_HEADERS})
qt5_wrap_ui(tcpsrc_FORMS_HEADERS ${tcpsrc_FORMS})

add_library(demodtcpsrc SHARED
	${tcpsrc_SOURCES}
	${tcpsrc_HEADERS_MOC}
	${tcpsrc_FORMS_HEADERS}
)

target_link_libraries(demodtcpsrc
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(demodtcpsrc Core Widgets Network)

install(TARGETS demodtcpsrc DESTINATION lib/plugins/channelrx)
