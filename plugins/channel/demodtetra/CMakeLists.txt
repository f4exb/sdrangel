project(tetra)

set(tetra_SOURCES
	tetrademod.cpp
	tetrademodgui.cpp
	tetraplugin.cpp
)

set(tetra_HEADERS
	tetrademod.h
	tetrademodgui.h
	tetraplugin.h
)

set(tetra_FORMS
	tetrademodgui.ui
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

#qt5_wrap_cpp(tetra_HEADERS_MOC ${tetra_HEADERS})
qt5_wrap_ui(tetra_FORMS_HEADERS ${tetra_FORMS})

add_library(demodtetra SHARED
	${tetra_SOURCES}
	${tetra_HEADERS_MOC}
	${tetra_FORMS_HEADERS}
)

target_link_libraries(demodtetra
	${QT_LIBRARIES}
	${OPENGL_LIBRARIES}
	sdrbase
)

qt5_use_modules(demodtetra Core Widgets OpenGL Multimedia)
