project(aircraft)

set(aircraft_SOURCES
    aircraft.cpp
    aircraftsettings.cpp
    aircraftplugin.cpp
    aircrafttracker.cpp
    aircraftdb.cpp
    aircraftwebapiadapter.cpp
    sqlite/sqlite3.c
)

set(aircraft_HEADERS
    aircraft.h
    aircraftsettings.h
    aircraftplugin.h
    aircrafttracker.h
    aircraftwebapiadapter.h
    sqlite/sqlite3.h
)

# Vendored SQLite amalgamation (public domain) for session persistence.
# Only used from the tracker thread, so thread safety is not needed.
set_source_files_properties(sqlite/sqlite3.c PROPERTIES
    COMPILE_DEFINITIONS "SQLITE_THREADSAFE=0;SQLITE_OMIT_LOAD_EXTENSION;SQLITE_OMIT_DEPRECATED"
    SKIP_AUTOGEN ON
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(aircraft_SOURCES
        ${aircraft_SOURCES}
        aircraftgui.cpp
        aircraftgui.ui
        aircrafticons.qrc
        aircrafttablemodels.cpp
        aircraftnotificationdialog.cpp
        aircraftnotificationdialog.ui
        aircraftsettingsdialog.cpp
        aircraftsettingsdialog.ui
    )
    set(aircraft_HEADERS
        ${aircraft_HEADERS}
        aircraftgui.h
        aircrafttablemodels.h
        aircraftnotificationdialog.h
        aircraftsettingsdialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featureaircraft)
    set(TARGET_LIB Qt::Widgets Qt::Positioning Qt::Charts)
    if(Qt${QT_DEFAULT_MAJOR_VERSION}TextToSpeech_FOUND)
        list(APPEND TARGET_LIB Qt::TextToSpeech)
    endif()
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featureaircraftsrv)
    set(TARGET_LIB Qt::Positioning)
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${aircraft_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AircraftPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${aircraft_SOURCES})
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
