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
    find_package(Threads REQUIRED)

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
        msfsassetconverter/attribstrip.cpp
        msfsassetconverter/attribstrip.h
        msfsassetconverter/animationmapper.cpp
        msfsassetconverter/animationmapper.h
        msfsassetconverter/batchconverter.cpp
        msfsassetconverter/batchconverter.h
        msfsassetconverter/bc7decoder.cpp
        msfsassetconverter/bc7decoder.h
        msfsassetconverter/converter.cpp
        msfsassetconverter/converter.h
        msfsassetconverter/ddsdecoder.cpp
        msfsassetconverter/ddsdecoder.h
        msfsassetconverter/fsltlresolver.cpp
        msfsassetconverter/fsltlresolver.h
        msfsassetconverter/hiddenparts.cpp
        msfsassetconverter/hiddenparts.h
        msfsassetconverter/nodestrip.cpp
        msfsassetconverter/nodestrip.h
        msfsassetconverter/ktx2writer.cpp
        msfsassetconverter/ktx2writer.h
        msfsassetconverter/portholes.cpp
        msfsassetconverter/portholes.h
        msfsassetconverter/primmerge.cpp
        msfsassetconverter/primmerge.h
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
        mapviewcoordinator.cpp
        cesiuminterface.cpp
        czml.cpp
        map.qrc
        mapicons.qrc
        mapaudio.qrc
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
        mapviewcoordinator.h
        cesiuminterface.h
        czml.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}map)
    set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Svg Qt::Positioning Qt::Location Threads::Threads)
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
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MapPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${map_SOURCES})
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

if(NOT SERVER_MODE)
    # Zstandard supercompression for the MSFS converter's KTX2 textures. Liveries are mostly
    # flat colour and compress about 8x losslessly: the shared texture cache went from 44 GB
    # to about 5 GB. A system libzstd is used where there is one; otherwise (always on
    # Windows, where external/windows has none) its compressor is built here from source
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND AND NOT WIN32)
        pkg_check_modules(ZSTD QUIET IMPORTED_TARGET libzstd)
    endif()
    if(ZSTD_FOUND)
        target_link_libraries(${TARGET_NAME} PRIVATE PkgConfig::ZSTD)
    else()
        include(FetchContent)
        # lib has no CMakeLists.txt, so this only downloads - zstd's own build would add
        # install rules for its library and headers to SDRangel's package
        FetchContent_Declare(zstd
            URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
            URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SOURCE_SUBDIR lib
        )
        FetchContent_MakeAvailable(zstd)
        # Only compression is used, so the decompressor and its assembly aren't needed
        file(GLOB MAP_ZSTD_SOURCES
            ${zstd_SOURCE_DIR}/lib/common/*.c
            ${zstd_SOURCE_DIR}/lib/compress/*.c
        )
        add_library(mapzstd STATIC ${MAP_ZSTD_SOURCES})
        set_target_properties(mapzstd PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            AUTOMOC OFF AUTOUIC OFF AUTORCC OFF
        )
        target_include_directories(mapzstd PUBLIC ${zstd_SOURCE_DIR}/lib)
        target_link_libraries(${TARGET_NAME} PRIVATE mapzstd)
    endif()
    target_compile_definitions(${TARGET_NAME} PRIVATE MSFS_CONVERTER_WITH_ZSTD)
endif()

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
