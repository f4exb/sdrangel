project(udpsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(udpsource_SOURCES
	udpsource.cpp
	udpsourcegui.cpp
	udpsourceplugin.cpp
	udpsourceudphandler.cpp
	udpsourcemsg.cpp
	udpsourcesettings.cpp
)

set(udpsource_HEADERS
	udpsource.h
	udpsourcegui.h
	udpsourceplugin.h
	udpsourceudphandler.h
	udpsourcemsg.h
	udpsourcesettings.h
)

set(udpsource_FORMS
	udpsourcegui.ui
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

qt5_wrap_ui(udpsource_FORMS_HEADERS ${udpsource_FORMS})

add_library(udpsource SHARED
	${udpsource_SOURCES}
	${udpsource_HEADERS_MOC}
	${udpsource_FORMS_HEADERS}
)

target_link_libraries(udpsource
	${QT_LIBRARIES}
	sdrbase
	sdrgui
	swagger
)

target_link_libraries(udpsource Qt5::Core Qt5::Widgets Qt5::Network)

install(TARGETS udpsource DESTINATION lib/plugins/channeltx)
