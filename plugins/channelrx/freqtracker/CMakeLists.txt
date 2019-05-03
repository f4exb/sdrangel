project(freqtracker)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(freqtracker_SOURCES
	freqtracker.cpp
	freqtrackergui.cpp
	freqtrackersettings.cpp
	freqtrackerplugin.cpp
)

set(freqtracker_HEADERS
	freqtracker.h
	freqtrackergui.h
	freqtrackersettings.h
	freqtrackerplugin.h
)

set(freqtracker_FORMS
	freqtrackergui.ui
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
qt5_wrap_ui(freqtracker_FORMS_HEADERS ${freqtracker_FORMS})

add_library(freqtracker SHARED
	${freqtracker_SOURCES}
	${freqtracker_HEADERS_MOC}
	${freqtracker_FORMS_HEADERS}
)

target_link_libraries(freqtracker
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

target_link_libraries(freqtracker Qt5::Core Qt5::Widgets)

install(TARGETS freqtracker DESTINATION lib/plugins/channelrx)
