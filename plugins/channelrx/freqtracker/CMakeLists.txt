project(freqtracker)

set(freqtracker_SOURCES
    freqtracker.cpp
    freqtrackerbaseband.cpp
    freqtrackersink.cpp
    freqtrackerreport.cpp
    freqtrackersettings.cpp
    freqtrackerwebapiadapter.cpp
	freqtrackerplugin.cpp
)

set(freqtracker_HEADERS
	freqtracker.h
    freqtrackerbaseband.h
    freqtrackersink.h
    freqtrackerreport.h
    freqtrackersettings.h
    freqtrackerwebapiadapter.h
	freqtrackerplugin.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(freqtracker_SOURCES
        ${freqtracker_SOURCES}
        freqtrackergui.cpp

	freqtrackergui.ui
    )
    set(freqtracker_HEADERS
        ${freqtracker_HEADERS}
        freqtrackergui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}freqtracker)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}freqtrackersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${freqtracker_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME FreqTrackerPlugin ${freqtracker_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
