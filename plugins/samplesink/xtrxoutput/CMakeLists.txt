project(xtrxoutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(xtrxoutput_SOURCES
	xtrxoutputgui.cpp
	xtrxoutput.cpp
	xtrxoutputplugin.cpp
	xtrxoutputsettings.cpp
	xtrxoutputthread.cpp
)

set(xtrxoutput_HEADERS
	xtrxoutputgui.h
	xtrxoutput.h
	xtrxoutputplugin.h
	xtrxoutputsettings.h
	xtrxoutputthread.h
)

set(xtrxoutput_FORMS
	xtrxoutputgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBXTRX_INCLUDE_DIRS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(xtrxoutput_FORMS_HEADERS ${xtrxoutput_FORMS})

add_library(outputxtrx SHARED
	${xtrxoutput_SOURCES}
	${xtrxoutput_HEADERS_MOC}
	${xtrxoutput_FORMS_HEADERS}
)

target_link_libraries(outputxtrx
	${QT_LIBRARIES}
	${LIBXTRX_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	xtrxdevice
)

target_link_libraries(outputxtrx Qt5::Core Qt5::Widgets)

install(TARGETS outputxtrx DESTINATION lib/plugins/samplesink)
