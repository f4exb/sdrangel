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

set(TARGET_NAME chanalyzer)
set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

add_library(${TARGET_NAME} SHARED
	${chanalyzer_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    Qt::Widgets
	sdrbase
	sdrgui
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
