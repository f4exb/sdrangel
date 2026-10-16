project(demoddvb)

set(demoddvb_SOURCES
    dvbdemod.cpp
    dvbdemodsettings.cpp
    dvbdemodbaseband.cpp
    dvbdemodsink.cpp
    dvbdemodplugin.cpp
    dvbdemodfreesat.cpp
    dvbdemodfreesattables.cpp
    dvbdemodtext.cpp
    dvbdemodepg.cpp
    dvbdemodwebapiadapter.cpp
)

set(demoddvb_HEADERS
    dvbdemod.h
    dvbdemodsettings.h
    dvbdemodbaseband.h
    dvbdemodsink.h
    dvbdemodplugin.h
    dvbdemodfreesat.h
    dvbdemodtext.h
    dvbdemodepg.h
    dvbdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}
)

if(NOT SERVER_MODE)
    # The service is decoded and displayed with FFmpeg, by sdrgui's DTVVideoPlayer. Without a GUI, the transport stream
    # can be sent over UDP
    set(demoddvb_SOURCES
        ${demoddvb_SOURCES}
        dvbdemodgui.cpp
        dvbdemodgui.ui
        dvbdemod.qrc
    )
    set(demoddvb_HEADERS
        ${demoddvb_HEADERS}
        dvbdemodgui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}demoddvb)
    set(TARGET_LIB Qt::Widgets)

    # HbbTV applications are run in Qt WebEngine's browser (Qt 5.15 or 6)
    if(Qt${QT_DEFAULT_MAJOR_VERSION}WebEngineWidgets_FOUND AND Qt${QT_DEFAULT_MAJOR_VERSION}QuickWidgets_FOUND)
        set(demoddvb_SOURCES ${demoddvb_SOURCES} dvbdemodhbbtv.cpp)
        set(demoddvb_HEADERS ${demoddvb_HEADERS} dvbdemodhbbtv.h)
        set(TARGET_LIB ${TARGET_LIB} Qt::WebEngineCore Qt::WebEngineWidgets Qt::QuickWidgets)
        set(DEMODDVB_HBBTV ON)
    else()
        message(STATUS "DVB demodulator: not running HbbTV applications (no Qt WebEngine)")
    endif()
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demoddvbsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demoddvb_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DVBDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demoddvb_SOURCES})
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
    modemdvb
)

if(DEMODDVB_HBBTV)
    target_compile_definitions(${TARGET_NAME} PRIVATE DVBDEMOD_HBBTV)
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
