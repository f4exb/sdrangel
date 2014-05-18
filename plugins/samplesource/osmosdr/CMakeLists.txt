project(osmosdr)

set(osmosdr_SOURCES
	osmosdrgui.cpp
	osmosdrinput.cpp
	osmosdrplugin.cpp
	osmosdrthread.cpp
)

set(osmosdr_HEADERS
	osmosdrgui.h
	osmosdrinput.h
	osmosdrplugin.h
	osmosdrthread.h
)

set(osmosdr_FORMS
	osmosdrgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${LIBOSMOSDR_INCLUDE_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

if(MSVC)
add_definitions(-DNOMINMAX)
endif()

#qt4_wrap_cpp(osmosdr_HEADERS_MOC ${osmosdr_HEADERS})
qt5_wrap_ui(osmosdr_FORMS_HEADERS ${osmosdr_FORMS})

add_library(inputosmosdr SHARED
	${osmosdr_SOURCES}
	${osmosdr_HEADERS_MOC}
	${osmosdr_FORMS_HEADERS}
)

target_link_libraries(inputosmosdr
	${QT_LIBRARIES}
	${LIBOSMOSDR_LIBRARIES}
	${LIBUSB_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputosmosdr Core Widgets OpenGL Multimedia)
