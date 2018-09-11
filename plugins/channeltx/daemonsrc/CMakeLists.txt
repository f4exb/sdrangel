project(daemonsrc)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(daemonsrc_SOURCES
	daemonsrc.cpp
	daemonsrcthread.cpp
	daemonsrcgui.cpp
	daemonsrcplugin.cpp
	daemonsrcsettings.cpp
)

set(daemonsrc_HEADERS
	daemonsrc.h
	daemonsrcthread.h
	daemonsrcgui.h
	daemonsrcplugin.h
	daemonsrcsettings.h
)

set(daemonsrc_FORMS
	daemonsrcgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CM256CC_INCLUDE_DIR}
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(daemonsrc_FORMS_HEADERS ${daemonsrc_FORMS})

add_library(daemonsrc SHARED
	${daemonsrc_SOURCES}
	${daemonsrc_HEADERS_MOC}
	${daemonsrc_FORMS_HEADERS}
)

target_link_libraries(daemonsrc
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

target_link_libraries(daemonsrc Qt5::Core Qt5::Widgets Qt5::Network)

install(TARGETS daemonsrc DESTINATION lib/plugins/channeltx)
