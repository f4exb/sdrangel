project(modnfm)

set(modnfm_SOURCES
	nfmmod.cpp
	nfmmodgui.cpp
	nfmmodplugin.cpp
	nfmmodsettings.cpp
)

set(modnfm_HEADERS
	nfmmod.h
	nfmmodgui.h
	nfmmodplugin.h
	nfmmodsettings.h
)

set(modnfm_FORMS
	nfmmodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(modnfm_FORMS_HEADERS ${modnfm_FORMS})

add_library(modnfm SHARED
	${modnfm_SOURCES}
	${modnfm_HEADERS_MOC}
	${modnfm_FORMS_HEADERS}
)

target_link_libraries(modnfm
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(modnfm Core Widgets)

install(TARGETS modnfm DESTINATION lib/plugins/channeltx)