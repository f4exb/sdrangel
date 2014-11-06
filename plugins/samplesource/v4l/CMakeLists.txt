project(v4l)

set(v4l_SOURCES
	v4lgui.cpp
	v4linput.cpp
	v4lplugin.cpp
	v4lthread.cpp
	v4lsource.cpp
)

set(rtlsdr_HEADERS
	v4lgui.h
	v4linput.h
	v4lplugin.h
	v4lthread.h
	v4lsource.h
)

set(v4l_FORMS
	v4lgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${LIBV4L2H}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(v4l_HEADERS_MOC ${v4l_HEADERS})
qt5_wrap_ui(v4l_FORMS_HEADERS ${v4l_FORMS})

add_library(inputv4l SHARED
	${v4l_SOURCES}
	${v4l_HEADERS_MOC}
	${v4l_FORMS_HEADERS}
)

target_link_libraries(inputv4l
	${QT_LIBRARIES}
 	${LIBV4L2}
	sdrbase
)

qt5_use_modules(inputv4l Core Widgets OpenGL Multimedia)
