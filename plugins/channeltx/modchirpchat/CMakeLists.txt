project(modchirpchat)

set(modchirpchat_SOURCES
	chirpchatmod.cpp
	chirpchatmodsettings.cpp
	chirpchatmodsource.cpp
	chirpchatmodbaseband.cpp
    chirpchatmodplugin.cpp
    chirpchatmodencoder.cpp
    chirpchatmodencodertty.cpp
    chirpchatmodencoderascii.cpp
    chirpchatmodencoderlora.cpp
    chirpchatmodwebapiadapter.cpp
)

set(modchirpchat_HEADERS
	chirpchatmod.h
	chirpchatmodsettings.h
	chirpchatmodsource.h
	chirpchatmodbaseband.h
    chirpchatmodplugin.h
    chirpchatmodencoder.h
    chirpchatmodencodertty.h
    chirpchatmodencoderascii.h
    chirpchatmodencoderlora.h
    chirpchatmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modchirpchat_SOURCES
        ${modchirpchat_SOURCES}
        chirpchatmodgui.cpp
        chirpchatmodgui.ui
    )
    set(modchirpchat_HEADERS
        ${modchirpchat_HEADERS}
    	chirpchatmodgui.h
    )

    set(TARGET_NAME modchirpchat)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modchirpchatsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${modchirpchat_SOURCES}
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
