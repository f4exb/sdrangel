project(fcd)

set(fcd_SOURCES
	fcdgui.cpp
	fcdinput.cpp
	fcdplugin.cpp
	fcdthread.cpp
	fcdsource.cpp
)

set(rtlsdr_HEADERS
	fcdgui.h
	fcdinput.h
	fcdplugin.h
	fcdthread.h
	fcdsource.h
)

set(fcd_FORMS
	fcdgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(fcd_HEADERS_MOC ${fcd_HEADERS})
qt5_wrap_ui(fcd_FORMS_HEADERS ${fcd_FORMS})

add_library(inputfcd SHARED
	${fcd_SOURCES}
	${fcd_HEADERS_MOC}
	${fcd_FORMS_HEADERS}
)

target_link_libraries(inputfcd
	${QT_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputfcd Core Widgets OpenGL Multimedia)
