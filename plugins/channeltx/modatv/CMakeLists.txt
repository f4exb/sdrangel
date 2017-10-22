project(modatv)

set(modatv_SOURCES
	atvmod.cpp
	atvmodgui.cpp
	atvmodplugin.cpp
	atvmodsettings.cpp
)

set(modatv_HEADERS
	atvmod.h
	atvmodgui.h
	atvmodplugin.h
	atvmodsettings.h
)

set(modatv_FORMS
	atvmodgui.ui
)

# OpenCV variables defined in /usr/share/OpenCV/OpenCVConfig.cmake (Ubuntu)

include_directories(
	.
	${OpenCV_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(modatv_FORMS_HEADERS ${modatv_FORMS})

add_library(modatv SHARED
	${modatv_SOURCES}
	${modatv_HEADERS_MOC}
	${modatv_FORMS_HEADERS}
)

target_link_libraries(modatv
    ${OpenCV_LIBS}
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(modatv Core Widgets)

install(TARGETS modatv DESTINATION lib/plugins/channeltx)