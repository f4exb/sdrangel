project(modwfm)

set(modwfm_SOURCES
	wfmmod.cpp
	wfmmodgui.cpp
	wfmmodplugin.cpp
	wfmmodsettings.cpp
)

set(modwfm_HEADERS
	wfmmod.h
	wfmmodgui.h
	wfmmodplugin.h
	wfmmodsettings.h
)

set(modwfm_FORMS
	wfmmodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(modwfm_FORMS_HEADERS ${modwfm_FORMS})

add_library(modwfm SHARED
	${modwfm_SOURCES}
	${modwfm_HEADERS_MOC}
	${modwfm_FORMS_HEADERS}
)

target_link_libraries(modwfm
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(modwfm Core Widgets)

install(TARGETS modwfm DESTINATION lib/plugins/channeltx)