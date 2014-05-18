project(rtlsdr)

set(rtlsdr_SOURCES
	rtlsdrgui.cpp
	rtlsdrinput.cpp
	rtlsdrplugin.cpp
	rtlsdrthread.cpp
)

set(rtlsdr_HEADERS
	rtlsdrgui.h
	rtlsdrinput.h
	rtlsdrplugin.h
	rtlsdrthread.h
)

set(rtlsdr_FORMS
	rtlsdrgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${LIBRTLSDR_INCLUDE_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(rtlsdr_HEADERS_MOC ${rtlsdr_HEADERS})
qt5_wrap_ui(rtlsdr_FORMS_HEADERS ${rtlsdr_FORMS})

add_library(inputrtlsdr SHARED
	${rtlsdr_SOURCES}
	${rtlsdr_HEADERS_MOC}
	${rtlsdr_FORMS_HEADERS}
)

target_link_libraries(inputrtlsdr
	${QT_LIBRARIES}
	${LIBRTLSDR_LIBRARIES}
	${LIBUSB_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputrtlsdr Core Widgets OpenGL Multimedia)
