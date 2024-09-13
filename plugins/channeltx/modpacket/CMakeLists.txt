project(modpacket)

set(modpacket_SOURCES
    packetmod.cpp
    packetmodbaseband.cpp
    packetmodsource.cpp
    packetmodplugin.cpp
    packetmodsettings.cpp
    packetmodwebapiadapter.cpp
)

set(modpacket_HEADERS
    packetmod.h
    packetmodbaseband.h
    packetmodsource.h
    packetmodplugin.h
    packetmodsettings.h
    packetmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modpacket_SOURCES
        ${modpacket_SOURCES}
        packetmodgui.cpp
        packetmodgui.ui
        packetmodbpfdialog.cpp
        packetmodbpfdialog.ui
        packetmodrepeatdialog.cpp
        packetmodrepeatdialog.ui
        packetmodtxsettingsdialog.cpp
        packetmodtxsettingsdialog.ui
    )
    set(modpacket_HEADERS
        ${modpacket_HEADERS}
        packetmodgui.h
        packetmodbpfdialog.h
        packetmodrepeatdialog.h
        packetmodtxsettingsdialog.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}modpacket)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modpacketsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modpacket_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME PacketModPlugin ${modpacket_SOURCES})
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
