project(xtrxinput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(xtrxinput_SOURCES
	xtrxinputgui.cpp
	xtrxinput.cpp
	xtrxinputplugin.cpp
	xtrxinputsettings.cpp
	xtrxinputthread.cpp
)

set(xtrxinput_HEADERS
	xtrxinputgui.h
	xtrxinput.h
	xtrxinputplugin.h
	xtrxinputsettings.h
	xtrxinputthread.h
)

set(xtrxinput_FORMS
	xtrxinputgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/devices
	${LIBXTRX_INCLUDE_DIRS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(xtrxinput_FORMS_HEADERS ${xtrxinput_FORMS})

add_library(inputxtrx SHARED
	${xtrxinput_SOURCES}
	${xtrxinput_HEADERS_MOC}
	${xtrxinput_FORMS_HEADERS}
)


target_link_libraries(inputxtrx
	${QT_LIBRARIES}
	${LIBXTRX_LIBRARIES}
	sdrbase
	sdrgui
	xtrxdevice
)

qt5_use_modules(inputxtrx Core Widgets)

install(TARGETS inputxtrx DESTINATION lib/plugins/samplesource)