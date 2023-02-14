project(map)

set(map_SOURCES
    map.cpp
    mapsettings.cpp
    mapplugin.cpp
    mapwebapiadapter.cpp
    osmtemplateserver.cpp
    ibpbeacon.cpp
    webserver.cpp
)

set(map_HEADERS
    map.h
    mapsettings.h
    mapplugin.h
    mapreport.h
    mapwebapiadapter.h
    osmtemplateserver.h
    beacon.h
    ibpbeacon.h
    webserver.h
)

if(Qt${QT_DEFAULT_MAJOR_VERSION}WebEngine_FOUND)
    add_compile_definitions(QT_WEBENGINE_FOUND)
    configure_file(mapguiwebengine.ui mapgui.ui)
else()
    configure_file(mapguinowebengine.ui mapgui.ui)
endif()
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_BINARY_DIR})

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Qt${QT_DEFAULT_MAJOR_VERSION}Gui_PRIVATE_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(map_SOURCES
        ${map_SOURCES}
        mapgui.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/mapgui.ui
        maplocationdialog.cpp
        maplocationdialog.ui
        mapmaidenheaddialog.cpp
        mapmaidenheaddialog.ui
        mapsettingsdialog.cpp
        mapsettingsdialog.ui
        mapbeacondialog.cpp
        mapbeacondialog.ui
        mapibpbeacondialog.cpp
        mapibpbeacondialog.ui
        mapradiotimedialog.cpp
        mapradiotimedialog.ui
        mapcolordialog.cpp
        mapmodel.cpp
        mapitem.cpp
        mapwebsocketserver.cpp
        cesiuminterface.cpp
        czml.cpp
        map.qrc
        icons.qrc
        cesium.qrc
        data.qrc
    )
    set(map_HEADERS
        ${map_HEADERS}
        mapgui.h
        maplocationdialog.h
        mapmaidenheaddialog.h
        mapsettingsdialog.h
        mapbeacondialog.h
        mapibpbeacon.h
        mapradiotimedialog.h
        mapcolordialog.h
        mapmodel.h
        mapitem.h
        mapwebsocketserver.h
        cesiuminterface.h
        czml.h
    )

    set(TARGET_NAME map)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

    if(Qt${QT_DEFAULT_MAJOR_VERSION}WebEngine_FOUND)
        set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Positioning Qt::Location Qt::WebEngine Qt::WebEngineCore Qt::WebEngineWidgets)
    else()
        set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Positioning Qt::Location)
    endif()
else()
    set(TARGET_NAME mapsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${map_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(WIN32)
  # Run deployqt for QtQuick etc
  include(DeployQt)
  windeployqt(${TARGET_NAME} ${SDRANGEL_BINARY_BIN_DIR} ${PROJECT_SOURCE_DIR}/map)
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
