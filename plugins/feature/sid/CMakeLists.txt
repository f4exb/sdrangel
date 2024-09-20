project(sid)

set(sid_SOURCES
    sid.cpp
    sidsettings.cpp
    sidplugin.cpp
    sidwebapiadapter.cpp
    sidworker.cpp
)

set(sid_HEADERS
    sid.h
    sidsettings.h
    sidplugin.h
    sidwebapiadapter.h
    sidworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(sid_SOURCES
        ${sid_SOURCES}
        sidgui.cpp
        sidgui.ui
        sidsettingsdialog.cpp
        sidsettingsdialog.ui
        sidaddchannelsdialog.cpp
        sidaddchannelsdialog.ui
        sidicons.qrc
    )
    set(sid_HEADERS
        ${sid_HEADERS}
        sidgui.h
        sidsettingsdialog.h
        sidaddchannelsdialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featuresid)
    set(TARGET_LIB Qt::Widgets Qt::Charts Qt::Multimedia Qt::MultimediaWidgets)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featuresidsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${sid_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME SIDPlugin ${sid_SOURCES})
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
    # Run deployqt for MultimediaWidgets, which isn't used in other plugins
    include(DeployQt)
    windeployqt(${TARGET_NAME} ${SDRANGEL_BINARY_BIN_DIR} "")
endif()


# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
