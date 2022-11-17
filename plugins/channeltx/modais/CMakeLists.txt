project(modais)

set(modais_SOURCES
    aismod.cpp
    aismodbaseband.cpp
    aismodsource.cpp
    aismodplugin.cpp
    aismodsettings.cpp
    aismodwebapiadapter.cpp
)

set(modais_HEADERS
    aismod.h
    aismodbaseband.h
    aismodsource.h
    aismodplugin.h
    aismodsettings.h
    aismodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modais_SOURCES
        ${modais_SOURCES}
        aismodgui.cpp
        aismodgui.ui
        aismodrepeatdialog.cpp
        aismodrepeatdialog.ui
        aismodtxsettingsdialog.cpp
        aismodtxsettingsdialog.ui
    )
    set(modais_HEADERS
        ${modais_HEADERS}
        aismodgui.h
        aismodrepeatdialog.h
        aismodtxsettingsdialog.h
    )
    set(TARGET_NAME modais)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modaissrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${modais_SOURCES}
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
