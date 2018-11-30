project(am)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(am_SOURCES
	amdemod.cpp
	amdemodgui.cpp
	amdemodsettings.cpp
	amdemodplugin.cpp
	amdemodssbdialog.cpp
)

set(am_HEADERS
	amdemod.h
	amdemodgui.h
	amdemodsettings.h
	amdemodplugin.h
	amdemodssbdialog.h
)

set(am_FORMS
	amdemodgui.ui
	amdemodssb.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client	
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
	sdrbase
	sdrgui
)

target_link_libraries(demodam Qt5::Core Qt5::Widgets)

install(TARGETS demodam DESTINATION lib/plugins/channelrx)
