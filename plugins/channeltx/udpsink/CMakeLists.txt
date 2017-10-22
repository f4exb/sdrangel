project(udpsink)

set(udpsink_SOURCES
	udpsink.cpp
	udpsinkgui.cpp
	udpsinkplugin.cpp
	udpsinkudphandler.cpp
	udpsinkmsg.cpp
	udpsinksettings.cpp
)

set(udpsink_HEADERS
	udpsink.h
	udpsinkgui.h
	udpsinkplugin.h
	udpsinkudphandler.h
	udpsinkmsg.h
	udpsinksettings.h
)

set(udpsink_FORMS
	udpsinkgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(udpsink_FORMS_HEADERS ${udpsink_FORMS})

add_library(modudpsink SHARED
	${udpsink_SOURCES}
	${udpsink_HEADERS_MOC}
	${udpsink_FORMS_HEADERS}
)

target_link_libraries(modudpsink
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(modudpsink Core Widgets Network)

install(TARGETS modudpsink DESTINATION lib/plugins/channeltx)
