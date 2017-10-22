project(am)

set(am_SOURCES
	amdemod.cpp
	amdemodgui.cpp
	amdemodsettings.cpp
	amdemodplugin.cpp
)

set(am_HEADERS
	amdemod.h
	amdemodgui.h
	amdemodsettings.h
	amdemodplugin.h
)

set(am_FORMS
	amdemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(nfm_HEADERS_MOC ${nfm_HEADERS})
qt5_wrap_ui(am_FORMS_HEADERS ${am_FORMS})

add_library(demodam SHARED
	${am_SOURCES}
	${am_HEADERS_MOC}
	${am_FORMS_HEADERS}
)

target_link_libraries(demodam
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(demodam Core Widgets)

install(TARGETS demodam DESTINATION lib/plugins/channelrx)