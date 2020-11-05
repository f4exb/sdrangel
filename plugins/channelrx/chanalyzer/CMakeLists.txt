project(chanalyzer)

set(chanalyzer_SOURCES
	chanalyzer.cpp
	chanalyzergui.cpp
	chanalyzerplugin.cpp
    chanalyzersettings.cpp
    chanalyzersink.cpp
    chanalyzerbaseband.cpp
	chanalyzerwebapiadapter.cpp
	chanalyzergui.ui
)

set(chanalyzer_HEADERS
	chanalyzer.h
	chanalyzergui.h
	chanalyzerplugin.h
    chanalyzersettings.h
    chanalyzersink.h
    chanalyzerbaseband.h
	chanalyzerwebapiadapter.h
)

include_directories(
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
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
