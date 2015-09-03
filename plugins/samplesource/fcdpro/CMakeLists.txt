project(fcdpro)

set(fcdpro_SOURCES
	fcdgui.cpp
	fcdinput.cpp
	fcdplugin.cpp
	fcdserializer.cpp
	fcdthread.cpp
)

set(fcdpro_HEADERS
	fcdgui.h
	fcdinput.h
	fcdplugin.h
	fcdserializer.h
	fcdthread.h
)

set(fcdpro_FORMS
	fcdgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${CMAKE_SOURCE_DIR}/fcdhid
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
	${LIBUSB_LIBRARIES}
	asound
	fcdhid
	sdrbase
)

qt5_use_modules(inputfcdpro Core Widgets OpenGL Multimedia)
