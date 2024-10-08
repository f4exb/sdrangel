project(remotetcpsink)

set(remotetcpsink_SOURCES
    remotetcpsink.cpp
    remotetcpsinkbaseband.cpp
    remotetcpsinksink.cpp
    remotetcpsinksettings.cpp
    remotetcpsinkwebapiadapter.cpp
    remotetcpsinkplugin.cpp
)

set(remotetcpsink_HEADERS
    remotetcpsink.h
    remotetcpsinkbaseband.h
    remotetcpsinksink.h
    remotetcpsinksettings.h
    remotetcpsinkwebapiadapter.h
    remotetcpsinkplugin.h
    remotetcpprotocol.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${FLAC_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(remotetcpsink_SOURCES
        ${remotetcpsink_SOURCES}
        remotetcpsinkgui.cpp
        remotetcpsinkgui.ui
        remotetcpsinksettingsdialog.cpp
        remotetcpsinksettingsdialog.ui
    )
    set(remotetcpsink_HEADERS
        ${remotetcpsink_HEADERS}
        remotetcpsinkgui.h
        remotetcpsinksettingsdialog.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}remotetcpsink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}remotetcpsinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${remotetcpsink_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RemoteTCPSinkPlugin ${remotetcpsink_SOURCES})
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
