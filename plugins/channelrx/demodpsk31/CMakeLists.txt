project(demodpsk31)

set(demodpsk31_SOURCES
    psk31demod.cpp
    psk31demodsettings.cpp
    psk31demodbaseband.cpp
    psk31demodsink.cpp
    psk31demodplugin.cpp
    psk31demodwebapiadapter.cpp
)

set(demodpsk31_HEADERS
    psk31demod.h
    psk31demodsettings.h
    psk31demodbaseband.h
    psk31demodsink.h
    psk31demodplugin.h
    psk31demodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demodpsk31_SOURCES
        ${demodpsk31_SOURCES}
        psk31demodgui.cpp
        psk31demodgui.ui
    )
    set(demodpsk31_HEADERS
        ${demodpsk31_HEADERS}
        psk31demodgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demodpsk31)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodpsk31srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodpsk31_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME PSK31DemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demodpsk31_SOURCES})
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
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
