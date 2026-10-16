project(demoddrm)

set(demoddrm_SOURCES
    drmdemod.cpp
    drmdemodbaseband.cpp
    drmdemodplugin.cpp
    drmdemodsettings.cpp
    drmdemodsink.cpp
    drmdemodwebapiadapter.cpp
)

set(demoddrm_HEADERS
    drmdemod.h
    drmdemodbaseband.h
    drmdemodplugin.h
    drmdemodsettings.h
    drmdemodsink.h
    drmdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    list(APPEND demoddrm_SOURCES
        drmdemodgui.cpp
        drmdemodgui.ui
    )
    list(APPEND demoddrm_HEADERS
        drmdemodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}demoddrm)
    set(TARGET_LIB Qt::Widgets)
    set(TARGET_LIB_GUI sdrgui)
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demoddrmsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demoddrm_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DRMDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demoddrm_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::Network
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    modemdrm
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER})
endif()
