project(hackrf)

set(hackrf_SOURCES
	hackrfgui.cpp
	hackrfinput.cpp
	hackrfplugin.cpp
	hackrfsettings.cpp
	hackrfthread.cpp
)

set(hackrf_HEADERS
	hackrfgui.h
	hackrfinput.h
	hackrfplugin.h
	hackrfsettings.h
	hackrfthread.h
)

set(hackrf_FORMS
	hackrfgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include-gpl
	${LIBHACKRF_INCLUDE_DIR}
)

#include(${QT_USE_FILE})
#add_definitions(${QT_DEFINITIONS})
add_definitions("${QT_DEFINITIONS} -DLIBHACKRF_DYN_RATES")
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(hackrf_HEADERS_MOC ${hackrf_HEADERS})
qt5_wrap_ui(hackrf_FORMS_HEADERS ${hackrf_FORMS})

add_library(inputhackrf SHARED
	${hackrf_SOURCES}
	${hackrf_HEADERS_MOC}
	${hackrf_FORMS_HEADERS}
)

target_link_libraries(inputhackrf
	${QT_LIBRARIES}
	${LIBHACKRF_LIBRARIES}
	${LIBUSB_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputhackrf Core Widgets OpenGL Multimedia)
