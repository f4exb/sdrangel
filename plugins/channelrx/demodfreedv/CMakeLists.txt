project(demodfreedv)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(freedv_SOURCES
	freedvdemod.cpp
	freedvdemodgui.cpp
	freedvdemodsettings.cpp
	freedvplugin.cpp
)

set(freedv_HEADERS
	freedvdemod.h
	freedvdemodgui.h
	freedvdemodsettings.h
	freedvplugin.h
)

set(freedv_FORMS
	freedvdemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
    ${CODEC2_INCLUDE_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(freedv_FORMS_HEADERS ${freedv_FORMS})

add_library(demodfreedv SHARED
	${freedv_SOURCES}
	${freedv_HEADERS_MOC}
	${freedv_FORMS_HEADERS}
)

target_link_libraries(demodfreedv
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
	${CODEC2_LIBRARIES}
)

target_link_libraries(demodfreedv Qt5::Core Qt5::Widgets)

install(TARGETS demodfreedv DESTINATION lib/plugins/channelrx)
