project(tcpsrc)

set(tcpsrc_SOURCES
	tcpsrc.cpp
	tcpsrcgui.cpp
	tcpsrcplugin.cpp
)

set(tcpsrc_HEADERS
	tcpsrc.h
	tcpsrcgui.h
	tcpsrcplugin.h
)

set(tcpsrc_FORMS
	tcpsrcgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${OPENGL_INCLUDE_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(tcpsrc_HEADERS_MOC ${tcpsrc_HEADERS})
qt5_wrap_ui(tcpsrc_FORMS_HEADERS ${tcpsrc_FORMS})

add_library(demodtcpsrc SHARED
	${tcpsrc_SOURCES}
	${tcpsrc_HEADERS_MOC}
	${tcpsrc_FORMS_HEADERS}
)

target_link_libraries(demodtcpsrc
	${QT_LIBRARIES}
	${OPENGL_LIBRARIES}
	sdrbase
)

qt5_use_modules(demodtcpsrc Core Widgets OpenGL Network)
