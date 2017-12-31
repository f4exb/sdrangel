project(bfm)

set(bfm_SOURCES
	bfmdemod.cpp
	bfmdemodgui.cpp
	bfmdemodsettings.cpp
	bfmplugin.cpp
	rdsdemod.cpp
	rdsdecoder.cpp
	rdsparser.cpp
	rdstmc.cpp
)

set(bfm_HEADERS
	bfmdemod.h
	bfmdemodgui.h
	bfmdemodsettings.h
	bfmplugin.h
	rdsdemod.h
	rdsdecoder.h
	rdsparser.h
	rdstmc.h
)

set_source_files_properties(rdstmc.cpp PROPERTIES COMPILE_FLAGS -fno-var-tracking-assignments)

set(bfm_FORMS
	bfmdemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

qt5_wrap_ui(bfm_FORMS_HEADERS ${bfm_FORMS})

add_library(demodbfm SHARED
	${bfm_SOURCES}
	${bfm_HEADERS_MOC}
	${bfm_FORMS_HEADERS}
)

target_link_libraries(demodbfm
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(demodbfm Core Widgets)

install(TARGETS demodbfm DESTINATION lib/plugins/channelrx)
