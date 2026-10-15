project(demodwefax)

set(demodwefax_SOURCES
    wefaxdecoder.cpp
    wefaxdemodsettings.cpp
    wefaxdemodsink.cpp
    wefaxdemodbaseband.cpp
    wefaxdemodimageworker.cpp
    wefaxslant.cpp
    wefaximageadjust.cpp
    wefaxtonemeter.cpp
    wefaxdemod.cpp
    wefaxdemodplugin.cpp
    wefaxdemodwebapiadapter.cpp
)

set(demodwefax_HEADERS
    wefaxdecoder.h
    wefaxdemodsettings.h
    wefaxdemodsink.h
    wefaxdemodbaseband.h
    wefaxdemodimageworker.h
    wefaxslant.h
    wefaximageadjust.h
    wefaxtonemeter.h
    wefaxdemod.h
    wefaxdemodplugin.h
    wefaxdemodwebapiadapter.h
)

include_directories(${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client)

if(NOT SERVER_MODE)
    list(APPEND demodwefax_SOURCES
        wefaxdemodgui.cpp
        wefaxdemodgui.ui
        wefaxdemodicons.qrc
    )
    list(APPEND demodwefax_HEADERS wefaxdemodgui.h)
    set(TARGET_NAME ${PLUGINS_PREFIX}demodwefax)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demodwefaxsrv)
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demodwefax_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME WefaxDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demodwefax_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    Qt::Gui
    sdrbase
    ${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

if(WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER})
endif()
