project(limesdroutput)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(limesdroutput_SOURCES
	limesdroutputgui.cpp
	limesdroutput.cpp
	limesdroutputplugin.cpp
	limesdroutputsettings.cpp
	limesdroutputthread.cpp
)

set(limesdroutput_HEADERS
	limesdroutputgui.h
	limesdroutput.h
	limesdroutputplugin.h
	limesdroutputsettings.h
	limesdroutputthread.h
)

set(limesdroutput_FORMS
	limesdroutputgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/devices
	${LIMESUITE_INCLUDE_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt4_wrap_cpp(limesdroutput_HEADERS_MOC ${limesdroutput_HEADERS})
qt5_wrap_ui(limesdroutput_FORMS_HEADERS ${limesdroutput_FORMS})

add_library(outputlimesdr SHARED
	${limesdroutput_SOURCES}
	${limesdroutput_HEADERS_MOC}
	${limesdroutput_FORMS_HEADERS}
)

target_link_libraries(outputlimesdr
	${QT_LIBRARIES}
	${LIMESUITE_LIBRARY}
	sdrbase
	limesdrdevice
)

qt5_use_modules(outputlimesdr Core Widgets)

install(TARGETS outputlimesdr DESTINATION lib/plugins/samplesink)
