project(am)

set(am_SOURCES
	amdemod.cpp
	amdemodgui.cpp
	amplugin.cpp
)

set(am_HEADERS
	amdemod.h
	amdemodgui.h
	amplugin.h
)

set(am_FORMS
	amdemodgui.ui
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

#qt5_wrap_cpp(nfm_HEADERS_MOC ${nfm_HEADERS})
qt5_wrap_ui(am_FORMS_HEADERS ${am_FORMS})

add_library(demodam SHARED
	${am_SOURCES}
	${am_HEADERS_MOC}
	${am_FORMS_HEADERS}
)

target_link_libraries(demodam
	${QT_LIBRARIES}
	${OPENGL_LIBRARIES}
	sdrbase
)

qt5_use_modules(demodam Core Widgets OpenGL Multimedia)