project(usb)

set(usb_SOURCES
	usbdemod.cpp
	usbdemodgui.cpp
	usbplugin.cpp
)

set(usb_HEADERS
	usbdemod.h
	usbdemodgui.h
	usbplugin.h
)

set(usb_FORMS
	usbdemodgui.ui
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

#qt5_wrap_cpp(usb_HEADERS_MOC ${usb_HEADERS})
qt5_wrap_ui(usb_FORMS_HEADERS ${usb_FORMS})

add_library(demodusb SHARED
	${usb_SOURCES}
	${usb_HEADERS_MOC}
	${usb_FORMS_HEADERS}
)

target_link_libraries(demodusb
	${QT_LIBRARIES}
	${OPENGL_LIBRARIES}
	sdrbase
)

qt5_use_modules(demodusb Core Widgets OpenGL Multimedia)
