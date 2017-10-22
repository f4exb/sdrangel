project(fcdpro)

set(fcdpro_SOURCES
	fcdprogui.cpp
	fcdproinput.cpp
	fcdproplugin.cpp
	fcdprosettings.cpp
	fcdprothread.cpp
)

set(fcdpro_HEADERS
	fcdprogui.h
	fcdproinput.h
	fcdproplugin.h
	fcdprosettings.h
	fcdprothread.h
)

set(fcdpro_FORMS
	fcdprogui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/fcdhid
	${CMAKE_SOURCE_DIR}/fcdlib
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(fcdpro_HEADERS_MOC ${fcdpro_HEADERS})
qt5_wrap_ui(fcdpro_FORMS_HEADERS ${fcdpro_FORMS})

add_library(inputfcdpro SHARED
	${fcdpro_SOURCES}
	${fcdpro_HEADERS_MOC}
	${fcdpro_FORMS_HEADERS}
)

target_link_libraries(inputfcdpro
	${QT_LIBRARIES}
	asound
	fcdhid
	fcdlib
	sdrbase
	sdrgui
)

qt5_use_modules(inputfcdpro Core Widgets)

install(TARGETS inputfcdpro DESTINATION lib/plugins/samplesource)