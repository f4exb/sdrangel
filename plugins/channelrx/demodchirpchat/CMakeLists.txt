project(chirpchat)

if (FT8_SUPPORT)
    set(chirpchatmod_FT8_LIB ft8)
    set(chirpchatmod_FT8_INCLUDE ${CMAKE_SOURCE_DIR}/ft8)
endif()

set(chirpchat_SOURCES
	chirpchatdemod.cpp
	chirpchatdemodsettings.cpp
	chirpchatdemodsink.cpp
	chirpchatdemodbaseband.cpp
	chirpchatplugin.cpp
	chirpchatdemoddecoder.cpp
	chirpchatdemoddecodertty.cpp
    chirpchatdemoddecoderascii.cpp
    chirpchatdemoddecoderlora.cpp
    chirpchatdemoddecoderft.cpp
	chirpchatdemodmsg.cpp
)

set(chirpchat_HEADERS
	chirpchatdemod.h
	chirpchatdemodsettings.h
	chirpchatdemodsink.h
	chirpchatdemodbaseband.h
	chirpchatdemoddecoder.h
	chirpchatdemoddecodertty.h
    chirpchatdemoddecoderascii.h
    chirpchatdemoddecoderlora.h
    chirpchatdemoddecoderft.h
	chirpchatdemodmsg.h
	chirpchatplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${chirpchatmod_FT8_INCLUDE}
)

if(NOT SERVER_MODE)
    set(chirpchat_SOURCES
        ${chirpchat_SOURCES}
        chirpchatdemodgui.cpp
        chirpchatdemodgui.ui
    )
    set(chirpchat_HEADERS
        ${chirpchat_HEADERS}
        chirpchatdemodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}demodchirpchat)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodchirpchatsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()


if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${chirpchat_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME ChirpChatDemodPlugin ${chirpchat_SOURCES})
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
