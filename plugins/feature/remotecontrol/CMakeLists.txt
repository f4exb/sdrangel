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

    set(TARGET_NAME featureremotecontrol)
    set(TARGET_LIB Qt::Widgets Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featureremotecontrolsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${remotecontrol_SOURCES}
)

target_link_libraries(${TARGET_NAME}
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
