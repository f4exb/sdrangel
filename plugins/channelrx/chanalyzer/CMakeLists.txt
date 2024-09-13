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

set(TARGET_NAME ${PLUGINS_PREFIX}chanalyzer)
set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${chanalyzer_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME ChannelAnalyzerPlugin ${chanalyzer_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
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
