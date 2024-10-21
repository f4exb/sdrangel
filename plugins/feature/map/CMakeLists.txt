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

# WebEngine on Qt5, WebEngineCore on Qt6
if(Qt${QT_DEFAULT_MAJOR_VERSION}WebEngine_FOUND OR Qt${QT_DEFAULT_MAJOR_VERSION}WebEngineCore_FOUND)
    add_compile_definitions(QT_WEBENGINE_FOUND)
    configure_file(mapguiwebengine.ui mapgui.ui)
else()
    configure_file(mapguinowebengine.ui mapgui.ui)
    message(STATUS "Not building 3D map (Qt${QT_DEFAULT_MAJOR_VERSION}WebEngine_FOUND=${Qt${QT_DEFAULT_MAJOR_VERSION}WebEngine_FOUND} Qt${QT_DEFAULT_MAJOR_VERSION}WebEngineCore_FOUND=${Qt${QT_DEFAULT_MAJOR_VERSION}WebEngineCore_FOUND})")
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
        #${CMAKE_CURRENT_BINARY_DIR}/mapgui.ui
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
        mapmodel.cpp
        mapitem.cpp
        mapwebsocketserver.cpp
        maptileserver.cpp
        cesiuminterface.cpp
        czml.cpp
        map.qrc
        mapicons.qrc
        cesium.qrc
        mapdata.qrc
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
        mapmodel.h
        mapitem.h
        mapwebsocketserver.h
        maptileserver.h
        cesiuminterface.h
        czml.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}map)
    set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Svg Qt::Positioning Qt::Location)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

    if(Qt${QT_DEFAULT_MAJOR_VERSION}WebEngine_FOUND)
        set(TARGET_LIB ${TARGET_LIB} Qt::WebEngine Qt::WebEngineCore Qt::WebEngineWidgets)
    elseif(Qt${QT_DEFAULT_MAJOR_VERSION}WebEngineCore_FOUND)
        set(TARGET_LIB ${TARGET_LIB} Qt::SvgWidgets Qt::WebEngineCore Qt::WebEngineWidgets)
    elseif(Qt6_FOUND)
        # Not on Qt5 Android?
        set(TARGET_LIB ${TARGET_LIB} Qt::SvgWidgets)
    endif()
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}mapsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${map_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MapPlugin ${map_SOURCES})
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

if(WIN32)
  # Run deployqt for QtQuick etc
  include(DeployQt)
  windeployqt(${TARGET_NAME} ${SDRANGEL_BINARY_BIN_DIR} ${PROJECT_SOURCE_DIR}/map)
endif()

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
