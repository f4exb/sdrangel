project(sdrdaemonsink)

set(sdrdaemonsink_SOURCES
	sdrdaemonsinkgui.cpp
	sdrdaemonsinkoutput.cpp
	sdrdaemonsinkplugin.cpp
	sdrdaemonsinksettings.cpp
	sdrdaemonsinkthread.cpp
    udpsinkfec.cpp
)

set(sdrdaemonsink_HEADERS
	sdrdaemonsinkgui.h
	sdrdaemonsinkoutput.h
	sdrdaemonsinkplugin.h
	sdrdaemonsinksettings.h
	sdrdaemonsinkthread.h
    udpsinkfec.h
)

set(sdrdaemonsink_FORMS
	sdrdaemonsinkgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_SOURCE_DIR}/devices
    ${CM256CC_INCLUDE_DIR}	
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(sdrdaemonsink_FORMS_HEADERS ${sdrdaemonsink_FORMS})

add_library(outputsdrdaemonsink SHARED
	${sdrdaemonsink_SOURCES}
	${sdrdaemonsink_HEADERS_MOC}
	${sdrdaemonsink_FORMS_HEADERS}
)

target_link_libraries(outputsdrdaemonsink
	${QT_LIBRARIES}
	sdrbase
    ${CM256CC_LIBRARIES}	
)

qt5_use_modules(outputsdrdaemonsink Core Widgets)

install(TARGETS outputsdrdaemonsink DESTINATION lib/plugins/samplesink)
