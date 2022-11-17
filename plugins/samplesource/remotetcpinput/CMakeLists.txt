project(remotetcpinput)

set(remotetcpinput_SOURCES
    remotetcpinputtcphandler.cpp
    remotetcpinput.cpp
    remotetcpinputsettings.cpp
    remotetcpinputwebapiadapter.cpp
    remotetcpinputplugin.cpp
)

set(remotetcpinput_HEADERS
    remotetcpinputtcphandler.h
    remotetcpinput.h
    remotetcpinputsettings.h
    remotetcpinputwebapiadapter.h
    remotetcpinputplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(remotetcpinput_SOURCES
        ${remotetcpinput_SOURCES}
        remotetcpinputgui.cpp
        remotetcpinputgui.ui
    )
    set(remotetcpinput_HEADERS
        ${remotetcpinput_HEADERS}
        remotetcpinputgui.h
    )

    set(TARGET_NAME inputtcpremote)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputtcpremotesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${remotetcpinput_SOURCES}
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
