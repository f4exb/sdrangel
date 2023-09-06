project(modpsk31)

set(modpsk31_SOURCES
    psk31mod.cpp
    psk31modbaseband.cpp
    psk31modsource.cpp
    psk31modplugin.cpp
    psk31modsettings.cpp
    psk31modwebapiadapter.cpp
)

set(modpsk31_HEADERS
    psk31mod.h
    psk31modbaseband.h
    psk31modsource.h
    psk31modplugin.h
    psk31modsettings.h
    psk31modwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modpsk31_SOURCES
        ${modpsk31_SOURCES}
        psk31modgui.cpp
        psk31modgui.ui
        psk31modrepeatdialog.cpp
        psk31modrepeatdialog.ui
        psk31modtxsettingsdialog.cpp
        psk31modtxsettingsdialog.ui
    )
    set(modpsk31_HEADERS
        ${modpsk31_HEADERS}
        psk31modgui.h
        psk31modrepeatdialog.h
        psk31modtxsettingsdialog.h
    )
    set(TARGET_NAME modpsk31)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modpsk31srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${modpsk31_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
