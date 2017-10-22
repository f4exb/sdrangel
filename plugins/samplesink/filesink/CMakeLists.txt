project(filesink)

set(filesink_SOURCES
	filesinkgui.cpp
	filesinkoutput.cpp
	filesinkplugin.cpp
	filesinksettings.cpp
	filesinkthread.cpp
)

set(filesink_HEADERS
	filesinkgui.h
	filesinkoutput.h
	filesinkplugin.h
	filesinksettings.h
	filesinkthread.h
)

set(filesink_FORMS
	filesinkgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(filesink_FORMS_HEADERS ${filesink_FORMS})

add_library(outputfilesink SHARED
	${filesink_SOURCES}
	${filesink_HEADERS_MOC}
	${filesink_FORMS_HEADERS}
)

target_link_libraries(outputfilesink
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(outputfilesink Core Widgets)

install(TARGETS outputfilesink DESTINATION lib/plugins/samplesink)
