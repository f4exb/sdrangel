project(demodnavtex)

set(demodnavtex_SOURCES
    navtexdemod.cpp
    navtexdemodsettings.cpp
    navtexdemodbaseband.cpp
    navtexdemodsink.cpp
    navtexdemodplugin.cpp
    navtexdemodwebapiadapter.cpp
)

set(demodnavtex_HEADERS
    navtexdemod.h
    navtexdemodsettings.h
    navtexdemodbaseband.h
    navtexdemodsink.h
    navtexdemodplugin.h
    navtexdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodnavtex_SOURCES
        ${demodnavtex_SOURCES}
        navtexdemodgui.cpp
        navtexdemodgui.ui
    )
    set(demodnavtex_HEADERS
        ${demodnavtex_HEADERS}
        navtexdemodgui.h
    )

    set(TARGET_NAME demodnavtex)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodnavtexsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demodnavtex_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
