project(modchirpchat)

if (FT8_SUPPORT)
    set(chirpchatmod_FT8_LIB ft8)
    set(chirpchatmod_FT8_INCLUDE ${CMAKE_SOURCE_DIR}/ft8)
endif()

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
    chirpchatmodencoderft.cpp
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
    chirpchatmodencoderft.h
    chirpchatmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${chirpchatmod_FT8_INCLUDE}
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

    set(TARGET_NAME ${PLUGINS_PREFIX}modchirpchat)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modchirpchatsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modchirpchat_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME ChirpChatModPlugin ${modchirpchat_SOURCES})
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
    ${chirpchatmod_FT8_LIB}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
