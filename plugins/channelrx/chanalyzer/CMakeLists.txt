project(chanalyzer)

set(chanalyzer_SOURCES
	chanalyzer.cpp
	chanalyzergui.cpp
	chanalyzerplugin.cpp
)

set(chanalyzer_HEADERS
	chanalyzer.h
	chanalyzergui.h
	chanalyzerplugin.h
)

set(chanalyzer_FORMS
	chanalyzergui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(chanalyzer_HEADERS_MOC ${chanalyzer_HEADERS})
qt5_wrap_ui(chanalyzer_FORMS_HEADERS ${chanalyzer_FORMS})

add_library(chanalyzer SHARED
	${chanalyzer_SOURCES}
	${chanalyzer_HEADERS_MOC}
	${chanalyzer_FORMS_HEADERS}
)

target_link_libraries(chanalyzer
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(chanalyzer Core Widgets )

install(TARGETS chanalyzer DESTINATION lib/plugins/channelrx)
