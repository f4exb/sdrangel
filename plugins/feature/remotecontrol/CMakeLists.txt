project(emotecontrol)

set(remotecontrol_SOURCES
    remotecontrol.cpp
    remotecontrolsettings.cpp
    remotecontrolplugin.cpp
    remotecontrolworker.cpp
)

set(remotecontrol_HEADERS
    remotecontrol.h
    remotecontrolsettings.h
    remotecontrolplugin.h
    remotecontrolworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(remotecontrol_SOURCES
        ${remotecontrol_SOURCES}
        remotecontrolgui.cpp
        remotecontrolgui.ui
        remotecontrolsettingsdialog.cpp
        remotecontrolsettingsdialog.ui
        remotecontroldevicedialog.cpp
        remotecontroldevicedialog.ui
        remotecontrolvisasensordialog.cpp
        remotecontrolvisasensordialog.ui
        remotecontrolvisacontroldialog.cpp
        remotecontrolvisacontroldialog.ui
    )
    set(remotecontrol_HEADERS
        ${remotecontrol_HEADERS}
        remotecontrolgui.h
        remotecontrolsettingsdialog.h
        remotecontroldevicedialog.h
        remotecontrolvisasensordialog.h
        remotecontrolvisacontroldialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featureremotecontrol)
    set(TARGET_LIB Qt::Widgets Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featureremotecontrolsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${remotecontrol_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RemoteControlPlugin ${remotecontrol_SOURCES})
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
