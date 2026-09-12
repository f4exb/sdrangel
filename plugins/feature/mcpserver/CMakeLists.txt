project(mcpserver)

# Embed every plugin readme so they can be served to MCP clients as documentation resources.
# The list is taken at configure time; re-run cmake after adding a plugin.
file(GLOB MCPSERVER_READMES
    ${CMAKE_SOURCE_DIR}/plugins/channelrx/*/readme.md
    ${CMAKE_SOURCE_DIR}/plugins/channeltx/*/readme.md
    ${CMAKE_SOURCE_DIR}/plugins/channelmimo/*/readme.md
    ${CMAKE_SOURCE_DIR}/plugins/samplesource/*/readme.md
    ${CMAKE_SOURCE_DIR}/plugins/samplesink/*/readme.md
    ${CMAKE_SOURCE_DIR}/plugins/samplemimo/*/readme.md
    ${CMAKE_SOURCE_DIR}/plugins/feature/*/readme.md
)
set(MCPSERVER_DOCS_QRC ${CMAKE_CURRENT_BINARY_DIR}/mcpdocs.qrc)
set(MCPSERVER_DOCS_QRC_CONTENT "<RCC>\n  <qresource prefix=\"/mcpdocs\">\n")
foreach(readme ${MCPSERVER_READMES})
    file(RELATIVE_PATH readme_alias ${CMAKE_SOURCE_DIR}/plugins ${readme})
    string(APPEND MCPSERVER_DOCS_QRC_CONTENT "    <file alias=\"${readme_alias}\">${readme}</file>\n")
endforeach()
string(APPEND MCPSERVER_DOCS_QRC_CONTENT "    <file alias=\"bandguide.md\">${CMAKE_CURRENT_SOURCE_DIR}/bandguide.md</file>\n")
string(APPEND MCPSERVER_DOCS_QRC_CONTENT "  </qresource>\n</RCC>\n")
file(WRITE ${MCPSERVER_DOCS_QRC} "${MCPSERVER_DOCS_QRC_CONTENT}")

set(mcpserver_SOURCES
    mcpserver.cpp
    mcpserversettings.cpp
    mcpserverplugin.cpp
    mcpserverwebapiadapter.cpp
    mcprequesthandler.cpp
    mcpprotocol.cpp
    mcptools.cpp
    mcpdocs.cpp
    mcpdatafeed.cpp
    mcpcapture.cpp
    mcpaudiocapture.cpp
    mcpstreams.cpp
    ${MCPSERVER_DOCS_QRC}
)

set(mcpserver_HEADERS
    mcpserver.h
    mcpserversettings.h
    mcpserverplugin.h
    mcpserverwebapiadapter.h
    mcprequesthandler.h
    mcpprotocol.h
    mcptools.h
    mcpdocs.h
    mcpdatafeed.h
    mcpcapture.h
    mcperror.h
    mcpstreams.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/httpserver
)

if(NOT SERVER_MODE)
    set(mcpserver_SOURCES
        ${mcpserver_SOURCES}
        mcpservergui.cpp
        mcpservergui.ui
        mcpcodexconfig.cpp
        mcpclaudeextension.cpp
    )
    set(mcpserver_HEADERS
        ${mcpserver_HEADERS}
        mcpservergui.h
        mcpcodexconfig.h
        mcpclaudeextension.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}featuremcpserver)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}featuremcpserversrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${mcpserver_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MCPServerPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${mcpserver_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    httpserver
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
