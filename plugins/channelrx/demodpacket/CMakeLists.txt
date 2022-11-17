project(demodpacket)

set(demodpacket_SOURCES
    packetdemod.cpp
    packetdemodsettings.cpp
    packetdemodbaseband.cpp
    packetdemodsink.cpp
    packetdemodplugin.cpp
    packetdemodwebapiadapter.cpp
)

set(demodpacket_HEADERS
    packetdemod.h
    packetdemodsettings.h
    packetdemodbaseband.h
    packetdemodsink.h
    packetdemodplugin.h
    packetdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodpacket_SOURCES
        ${demodpacket_SOURCES}
        packetdemodgui.cpp
        packetdemodgui.ui
    )
    set(demodpacket_HEADERS
        ${demodpacket_HEADERS}
        packetdemodgui.h
    )

    set(TARGET_NAME demodpacket)
    set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Positioning)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodpacketsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demodpacket_SOURCES}
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
