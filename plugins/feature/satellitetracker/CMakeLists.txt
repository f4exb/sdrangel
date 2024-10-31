project(satellitetracker)

set(satellitetracker_SOURCES
    satellitetracker.cpp
    satellitetrackersettings.cpp
    satellitetrackerplugin.cpp
    satellitetrackerworker.cpp
    satellitetrackerwebapiadapter.cpp
    satellitetrackersgp4.cpp
)

set(satellitetracker_HEADERS
    satellitetracker.h
    satellitetrackersettings.h
    satellitetrackerplugin.h
    satellitetrackerreport.h
    satellitetrackerworker.h
    satellitetrackerwebapiadapter.h
    satellitetrackersgp4.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${SGP4_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(satellitetracker_SOURCES
        ${satellitetracker_SOURCES}
        satellitetrackergui.cpp
        satellitetrackergui.ui
        satellitetrackersettingsdialog.cpp
        satellitetrackersettingsdialog.ui
        satellitetracker.qrc
        satelliteselectiondialog.cpp
        satelliteselectiondialog.ui
        satelliteradiocontroldialog.cpp
        satelliteradiocontroldialog.ui
        satellitedevicesettingsgui.cpp
    )
    set(satellitetracker_HEADERS
        ${satellitetracker_HEADERS}
        satellitetrackergui.h
        satellitetrackersettingsdialog.h
        satelliteselectiondialog.h
        satelliteradiocontroldialog.h
        satellitedevicesettingsgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featuresatellitetracker)
    set(TARGET_LIB Qt::Widgets Qt::Positioning Qt::Charts Qt::TextToSpeech)
    if(Qt${QT_DEFAULT_MAJOR_VERSION}TextToSpeech_FOUND)
        list(APPEND TARGET_LIB Qt::TextToSpeech)
    endif()
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featuresatellitetrackersrv)
    set(TARGET_LIB Qt::Positioning Qt::Charts)
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${satellitetracker_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME SatelliteTrackerPlugin ${satellitetracker_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(SGP4_EXTERNAL)
    add_dependencies(${TARGET_NAME} sgp4)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    ${SGP4_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(WIN32)
  # Run deployqt for Charts and TextToSpeech etc
  include(DeployQt)
  windeployqt(${TARGET_NAME} ${SDRANGEL_BINARY_BIN_DIR} ${PROJECT_SOURCE_DIR}/aprs)
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
