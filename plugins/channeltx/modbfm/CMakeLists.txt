project(modbfm)

set(modbfm_SOURCES
    bfmmod.cpp
    bfmmodbaseband.cpp
    bfmmodsource.cpp
    rdsencoder.cpp
    lookaheadlimiter.cpp
    bfmmodsettings.cpp
    bfmmodwebapiadapter.cpp
    bfmmodplugin.cpp
)

set(modbfm_HEADERS
    bfmmod.h
    bfmmodbaseband.h
    bfmmodsource.h
    rdsencoder.h
    lookaheadlimiter.h
    bfmmodsettings.h
    bfmmodwebapiadapter.h
    bfmmodplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    list(APPEND modbfm_SOURCES
        bfmmodgui.cpp
        bfmmodgui.ui
    )
    list(APPEND modbfm_HEADERS bfmmodgui.h)
    set(TARGET_NAME ${PLUGINS_PREFIX}modbfm)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}modbfmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${modbfm_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME BFMModPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${modbfm_SOURCES})
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
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER})
endif()

