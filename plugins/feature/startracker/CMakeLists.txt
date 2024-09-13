project(startracker)

set(startracker_SOURCES
    startracker.cpp
    startrackersettings.cpp
    startrackerplugin.cpp
    startrackerworker.cpp
    startrackerwebapiadapter.cpp
    startracker150mhzfits.qrc
    startracker408mhzfits.qrc
    startracker1420mhzfits.qrc
)

set(startracker_HEADERS
    startracker.h
    startrackersettings.h
    startrackerplugin.h
    startrackerreport.h
    startrackerworker.h
    startrackerwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(startracker_SOURCES
        ${startracker_SOURCES}
        startrackergui.cpp
        startrackergui.ui
        startrackersettingsdialog.cpp
        startrackersettingsdialog.ui
        startracker.qrc
        startracker150mhz.qrc
        startracker408mhz.qrc
        startracker1420mhz.qrc
        startrackermilkyway.qrc
    )
    set(startracker_HEADERS
        ${startracker_HEADERS}
        startrackergui.h
        startrackersettingsdialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featurestartracker)
    set(TARGET_LIB Qt::Widgets Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featurestartrackersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${startracker_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME StarTrackerPlugin ${startracker_SOURCES})
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
