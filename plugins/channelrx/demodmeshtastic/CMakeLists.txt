project(meshtastic)

if (FT8_SUPPORT)
    set(meshtastic_FT8_LIB ft8)
    set(meshtastic_FT8_INCLUDE ${CMAKE_SOURCE_DIR}/ft8)
endif()

set(meshtastic_SOURCES
	meshtasticdemod.cpp
	meshtasticdemodsettings.cpp
	meshtasticdemodsink.cpp
	meshtasticdemodbaseband.cpp
	meshtasticplugin.cpp
	meshtasticdemoddecoder.cpp
	meshtasticdemoddecodertty.cpp
    meshtasticdemoddecoderascii.cpp
    meshtasticdemoddecoderlora.cpp
    meshtasticdemoddecoderft.cpp
	meshtasticdemodmsg.cpp
    meshtasticdemodwebapiadapter.cpp
)

set(meshtastic_HEADERS
	meshtasticdemod.h
	meshtasticdemodsettings.h
	meshtasticdemodsink.h
	meshtasticdemodbaseband.h
	meshtasticdemoddecoder.h
	meshtasticdemoddecodertty.h
    meshtasticdemoddecoderascii.h
    meshtasticdemoddecoderlora.h
    meshtasticdemoddecoderft.h
	meshtasticdemodmsg.h
	meshtasticplugin.h
    meshtasticdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${meshtastic_FT8_INCLUDE}
    ${CMAKE_SOURCE_DIR}/modemmeshtastic
)

if(NOT SERVER_MODE)
    set(meshtastic_SOURCES
        ${meshtastic_SOURCES}
        meshtasticdemodgui.cpp
        meshtasticdemodgui.ui
    )
    set(meshtastic_HEADERS
        ${meshtastic_HEADERS}
        meshtasticdemodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}demodmeshtastic)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodmeshtasticsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()


if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${meshtastic_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MeshtasticPlugin ${meshtastic_SOURCES})
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
    ${meshtastic_FT8_LIB}
    modemmeshtastic
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
