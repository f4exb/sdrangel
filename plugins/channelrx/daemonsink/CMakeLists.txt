project(daemonsink)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(daemonsink_SOURCES
	daemonsink.cpp
#	daemonsinkgui.cpp
	daemonsinksettings.cpp
	daemonsinkthread.cpp
#	daemonsinkplugin.cpp
)

set(daemonsink_HEADERS
	daemonsink.h
#	daemonsinkgui.h
	daemonsinksettings.h
	daemonsinkthread.h
#	daemonsinkplugin.h
)

set(daemonsink_FORMS
	daemonsinkgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/sdrdaemon
	${CM256CC_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client		
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(daemonsink_FORMS_HEADERS ${daemonsink_FORMS})

add_library(daemonsink SHARED
	${daemonsink_SOURCES}
	${daemonsink_HEADERS_MOC}
	${daemonsink_FORMS_HEADERS}
)

target_link_libraries(daemonsink
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	sdrdaemon
	sdrgui
	swagger
)

target_link_libraries(daemonsink Qt5::Core Qt5::Widgets)

install(TARGETS daemonsink DESTINATION lib/plugins/channelrx)
