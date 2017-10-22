project(fcdproplus)

set(fcdproplus_SOURCES
	fcdproplusgui.cpp
	fcdproplusinput.cpp
	fcdproplusplugin.cpp
	fcdproplussettings.cpp
	fcdproplusthread.cpp
)

set(fcdproplus_HEADERS
	fcdproplusgui.h
	fcdproplusinput.h
	fcdproplusplugin.h
	fcdproplussettings.h
	fcdproplusthread.h
)

set(fcdproplus_FORMS
	fcdproplusgui.ui
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

#qt4_wrap_cpp(fcdproplus_HEADERS_MOC ${fcdproplus_HEADERS})
qt5_wrap_ui(fcdproplus_FORMS_HEADERS ${fcdproplus_FORMS})

add_library(inputfcdproplus SHARED
	${fcdproplus_SOURCES}
	${fcdproplus_HEADERS_MOC}
	${fcdproplus_FORMS_HEADERS}
)

target_link_libraries(inputfcdproplus
	${QT_LIBRARIES}
	asound
	fcdhid
	fcdlib
	sdrbase
	sdrgui
)

qt5_use_modules(inputfcdproplus Core Widgets)

install(TARGETS inputfcdproplus DESTINATION lib/plugins/samplesource)
