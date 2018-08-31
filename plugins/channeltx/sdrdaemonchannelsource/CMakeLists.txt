project(sdrdaemonchannelsource)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(sdrdaemonchannelsource_SOURCES
	sdrdaemonchannelsource.cpp
	sdrdaemonchannelsourcethread.cpp
	sdrdaemonchannelsourcegui.cpp
	sdrdaemonchannelsourceplugin.cpp
	sdrdaemonchannelsourcesettings.cpp
)

set(sdrdaemonchannelsource_HEADERS
	sdrdaemonchannelsource.h
	sdrdaemonchannelsourcethread.h
	sdrdaemonchannelsourcegui.h
	sdrdaemonchannelsourceplugin.h
	sdrdaemonchannelsourcesettings.h
)

set(sdrdaemonchannelsource_FORMS
	sdrdaemonchannelsourcegui.ui
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

qt5_wrap_ui(sdrdaemonchannelsource_FORMS_HEADERS ${sdrdaemonchannelsource_FORMS})

add_library(sdrdaemonchannelsource SHARED
	${sdrdaemonchannelsource_SOURCES}
	${sdrdaemonchannelsource_HEADERS_MOC}
	${sdrdaemonchannelsource_FORMS_HEADERS}
)

target_link_libraries(sdrdaemonchannelsource
	${QT_LIBRARIES}
	${CM256CC_LIBRARIES}
	sdrbase
	sdrdaemon
	sdrgui
	swagger
)

target_link_libraries(sdrdaemonchannelsource Qt5::Core Qt5::Widgets Qt5::Network)

install(TARGETS sdrdaemonchannelsource DESTINATION lib/plugins/channeltx)
