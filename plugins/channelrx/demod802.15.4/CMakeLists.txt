project(demod_ieee_802_15_4)

set(demod_ieee_802_15_4_SOURCES
    ieee_802_15_4_demod.cpp
    ieee_802_15_4_demodsettings.cpp
    ieee_802_15_4_demodbaseband.cpp
    ieee_802_15_4_demodsink.cpp
    ieee_802_15_4_demodreceiver.cpp
    ieee_802_15_4_demodplugin.cpp
    ieee_802_15_4_demodwebapiadapter.cpp
)

set(demod_ieee_802_15_4_HEADERS
    ieee_802_15_4_demod.h
    ieee_802_15_4_demodsettings.h
    ieee_802_15_4_demodbaseband.h
    ieee_802_15_4_demodsink.h
    ieee_802_15_4_demodreceiver.h
    ieee_802_15_4_demodplugin.h
    ieee_802_15_4_demodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demod_ieee_802_15_4_SOURCES
        ${demod_ieee_802_15_4_SOURCES}
        ieee_802_15_4_demodgui.cpp
        ieee_802_15_4_demodgui.ui
    )
    set(demod_ieee_802_15_4_HEADERS
        ${demod_ieee_802_15_4_HEADERS}
        ieee_802_15_4_demodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}demodieee_802_15_4)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodieee_802_15_4srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demod_ieee_802_15_4_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME IEEE_802_15_4_DemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demod_ieee_802_15_4_SOURCES})
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

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
