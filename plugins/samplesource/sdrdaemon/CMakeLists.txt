project(sdrdaemon)

set(sdrdaemon_SOURCES
	sdrdaemongui.cpp
	sdrdaemoninput.cpp
	sdrdaemonplugin.cpp
	sdrdaemonthread.cpp
)

set(sdrdaemon_HEADERS
	sdrdaemongui.h
	sdrdaemoninput.h
	sdrdaemonplugin.h
	sdrdaemonthread.h
)

set(sdrdaemon_FORMS
	sdrdaemongui.ui
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

#qt4_wrap_cpp(sdrdaemon_HEADERS_MOC ${sdrdaemon_HEADERS})
qt5_wrap_ui(sdrdaemon_FORMS_HEADERS ${sdrdaemon_FORMS})

add_library(inputsdrdaemon SHARED
	${sdrdaemon_SOURCES}
	${sdrdaemon_HEADERS_MOC}
	${sdrdaemon_FORMS_HEADERS}
)

target_link_libraries(inputsdrdaemon
	${QT_LIBRARIES}
	${LIBUSB_LIBRARIES}
	sdrbase
)

qt5_use_modules(inputsdrdaemon Core Widgets OpenGL Multimedia)
