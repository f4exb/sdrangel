project(mod_ieee_802_15_4)

set(mod_ieee_802_15_4_SOURCES
    ieee_802_15_4_mod.cpp
    ieee_802_15_4_modbaseband.cpp
    ieee_802_15_4_modsource.cpp
    ieee_802_15_4_modplugin.cpp
    ieee_802_15_4_modsettings.cpp
    ieee_802_15_4_modwebapiadapter.cpp
)

set(mod_ieee_802_15_4_HEADERS
    ieee_802_15_4_mod.h
    ieee_802_15_4_modbaseband.h
    ieee_802_15_4_modsource.h
    ieee_802_15_4_modplugin.h
    ieee_802_15_4_modsettings.h
    ieee_802_15_4_modwebapiadapter.h
    ieee_802_15_4_macframe.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(mod_ieee_802_15_4_SOURCES
        ${mod_ieee_802_15_4_SOURCES}
        ieee_802_15_4_modgui.cpp
        ieee_802_15_4_modgui.ui
        ieee_802_15_4_modrepeatdialog.cpp
        ieee_802_15_4_modrepeatdialog.ui
        ieee_802_15_4_modtxsettingsdialog.cpp
        ieee_802_15_4_modtxsettingsdialog.ui
    )
    set(mod_ieee_802_15_4_HEADERS
        ${mod_ieee_802_15_4_HEADERS}
        ieee_802_15_4_modgui.h
        ieee_802_15_4_modrepeatdialog.h
        ieee_802_15_4_modtxsettingsdialog.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}modieee_802_15_4)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modieee_802_15_4srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${mod_ieee_802_15_4_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME IEEE_802_15_4_ModPlugin ${mod_ieee_802_15_4_SOURCES})
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
