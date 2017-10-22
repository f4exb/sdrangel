project(modam)

set(modam_SOURCES
	ammod.cpp
	ammodgui.cpp
	ammodplugin.cpp
	ammodsettings.cpp
)

set(modam_HEADERS
	ammod.h
	ammodgui.h
	ammodplugin.h
	ammodsettings.h
)

set(modam_FORMS
	ammodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(modam_FORMS_HEADERS ${modam_FORMS})

add_library(modam SHARED
	${modam_SOURCES}
	${modam_HEADERS_MOC}
	${modam_FORMS_HEADERS}
)

target_link_libraries(modam
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(modam Core Widgets)

install(TARGETS modam DESTINATION lib/plugins/channeltx)