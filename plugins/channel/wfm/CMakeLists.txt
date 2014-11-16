project(wfm)

set(wfm_SOURCES
	wfmdemod.cpp
	wfmdemodgui.cpp
	wfmplugin.cpp
)

set(wfm_HEADERS
	wfmdemod.h
	wfmdemodgui.h
	wfmplugin.h
)

set(wfm_FORMS
	wfmdemodgui.ui
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

#qt5_wrap_cpp(wfm_HEADERS_MOC ${wfm_HEADERS})
qt5_wrap_ui(wfm_FORMS_HEADERS ${wfm_FORMS})

add_library(demodwfm SHARED
	${wfm_SOURCES}
	${wfm_HEADERS_MOC}
	${wfm_FORMS_HEADERS}
)

target_link_libraries(demodwfm
	${QT_LIBRARIES}
	${OPENGL_LIBRARIES}
	sdrbase
)

qt5_use_modules(demodwfm Core Widgets OpenGL Multimedia)
