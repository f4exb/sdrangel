project(dsddemod)

set(dsddemod_SOURCES
	dsddemod.cpp
	dsddemodgui.cpp
	dsddemodplugin.cpp
	dsddecoder.cpp
)

set(dsddemod_HEADERS
	dsddemod.h
	dsddemodgui.h
	dsddemodplugin.h
	dsddecoder.h
)

set(dsddemod_FORMS
	dsddemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${LIBDSDCC_INCLUDE_DIR}
	${LIBMBE_INCLUDE_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(dsddemod_FORMS_HEADERS ${dsddemod_FORMS})

add_library(demoddsd SHARED
	${dsddemod_SOURCES}
	${dsddemod_HEADERS_MOC}
	${dsddemod_FORMS_HEADERS}
)

target_link_libraries(demoddsd
	${QT_LIBRARIES}
	sdrbase
	${LIBDSDCC_LIBRARIES}
	${LIBMBE_LIBRARY}
)

qt5_use_modules(demoddsd Core Widgets OpenGL Multimedia)

install(TARGETS demoddsd DESTINATION lib/plugins/channel)