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
    set(TARGET_NAME ${PLUGINS_PREFIX}modrtty)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modrttysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modrtty_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RttyModPlugin ${modrtty_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
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
