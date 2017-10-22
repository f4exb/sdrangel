project(ssb)

set(ssb_SOURCES
	ssbdemod.cpp
	ssbdemodgui.cpp
	ssbdemodsettings.cpp
	ssbplugin.cpp
)

set(ssb_HEADERS
	ssbdemod.h
	ssbdemodgui.h
	ssbdemodsettings.h
	ssbplugin.h
)

set(ssb_FORMS
	ssbdemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(ssb_HEADERS_MOC ${ssb_HEADERS})
qt5_wrap_ui(ssb_FORMS_HEADERS ${ssb_FORMS})

add_library(demodssb SHARED
	${ssb_SOURCES}
	${ssb_HEADERS_MOC}
	${ssb_FORMS_HEADERS}
)

target_link_libraries(demodssb
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(demodssb Core Widgets)

install(TARGETS demodssb DESTINATION lib/plugins/channelrx)
