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
    spyserver.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${FLAC_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
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

    set(TARGET_NAME ${PLUGINS_PREFIX}inputtcpremote)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputtcpremotesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${remotetcpinput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RemoteTCPInputPlugin ${remotetcpinput_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::WebSockets
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${FLAC_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
