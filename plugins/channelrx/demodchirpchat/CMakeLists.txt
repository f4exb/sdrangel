project(chirpchat)

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
	chirpchatdemodmsg.h
	chirpchatplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
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
    set(TARGET_NAME demodchirpchat)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodchirpchatsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${chirpchat_SOURCES}
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
