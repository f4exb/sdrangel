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
)

if(NOT SERVER_MODE)
    set(remotetcpsink_SOURCES
        ${remotetcpsink_SOURCES}
        remotetcpsinkgui.cpp
        remotetcpsinkgui.ui
    )
    set(remotetcpsink_HEADERS
        ${remotetcpsink_HEADERS}
        remotetcpsinkgui.h
    )
    set(TARGET_NAME remotetcpsink)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME remotetcpsinksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${remotetcpsink_SOURCES}
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
