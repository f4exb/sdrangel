project(modmeshtastic)

if (FT8_SUPPORT)
    set(meshtasticmod_FT8_LIB ft8)
    set(meshtasticmod_FT8_INCLUDE ${CMAKE_SOURCE_DIR}/ft8)
endif()

set(modmeshtastic_SOURCES
	meshtasticmod.cpp
	meshtasticmodsettings.cpp
	meshtasticmodsource.cpp
	meshtasticmodbaseband.cpp
    meshtasticmodplugin.cpp
    meshtasticmodencoder.cpp
    meshtasticmodencodertty.cpp
    meshtasticmodencoderascii.cpp
    meshtasticmodencoderlora.cpp
    meshtasticmodencoderft.cpp
    meshtasticmodwebapiadapter.cpp
    ${CMAKE_SOURCE_DIR}/plugins/meshtasticcommon/meshtasticpacket.cpp
)

set(modmeshtastic_HEADERS
	meshtasticmod.h
	meshtasticmodsettings.h
	meshtasticmodsource.h
	meshtasticmodbaseband.h
    meshtasticmodplugin.h
    meshtasticmodencoder.h
    meshtasticmodencodertty.h
    meshtasticmodencoderascii.h
    meshtasticmodencoderlora.h
    meshtasticmodencoderft.h
    meshtasticmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${meshtasticmod_FT8_INCLUDE}
    ${CMAKE_SOURCE_DIR}/plugins/meshtasticcommon
)

if(NOT SERVER_MODE)
    set(modmeshtastic_SOURCES
        ${modmeshtastic_SOURCES}
        meshtasticmodgui.cpp
        meshtasticmodgui.ui
    )
    set(modmeshtastic_HEADERS
        ${modmeshtastic_HEADERS}
        meshtasticmodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}modmeshtastic)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modmeshtasticsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modmeshtastic_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MeshtasticModPlugin ${modmeshtastic_SOURCES})
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
    ${meshtasticmod_FT8_LIB}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
