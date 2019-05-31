project(chanalyzer)

set(chanalyzer_SOURCES
	chanalyzer.cpp
	chanalyzergui.cpp
	chanalyzerplugin.cpp
	chanalyzersettings.cpp

	chanalyzergui.ui
)

set(chanalyzer_HEADERS
	chanalyzer.h
	chanalyzergui.h
	chanalyzerplugin.h
	chanalyzersettings.h
)

include_directories(
	${Boost_INCLUDE_DIRS}
)

add_library(chanalyzer SHARED
	${chanalyzer_SOURCES}
)

target_link_libraries(chanalyzer
        Qt5::Core
        Qt5::Widgets
	sdrbase
	sdrgui
)

install(TARGETS chanalyzer DESTINATION ${INSTALL_PLUGINS_DIR})
