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

    set(TARGET_NAME ${PLUGINS_PREFIX}demodnavtex)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodnavtexsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodnavtex_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME NavtexDemodPlugin ${demodnavtex_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
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
