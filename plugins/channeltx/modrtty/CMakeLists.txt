project(modrtty)

set(modrtty_SOURCES
    rttymod.cpp
    rttymodbaseband.cpp
    rttymodsource.cpp
    rttymodplugin.cpp
    rttymodsettings.cpp
    rttymodwebapiadapter.cpp
)

set(modrtty_HEADERS
    rttymod.h
    rttymodbaseband.h
    rttymodsource.h
    rttymodplugin.h
    rttymodsettings.h
    rttymodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modrtty_SOURCES
        ${modrtty_SOURCES}
        rttymodgui.cpp
        rttymodgui.ui
        rttymodrepeatdialog.cpp
        rttymodrepeatdialog.ui
        rttymodtxsettingsdialog.cpp
        rttymodtxsettingsdialog.ui
    )
    set(modrtty_HEADERS
        ${modrtty_HEADERS}
        rttymodgui.h
        rttymodrepeatdialog.h
        rttymodtxsettingsdialog.h
    )
    set(TARGET_NAME modrtty)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modrttysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${modrtty_SOURCES}
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
