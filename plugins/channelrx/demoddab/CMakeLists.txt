project(demoddab)

set(demoddab_SOURCES
    dabdemod.cpp
    dabdemodsettings.cpp
    dabdemodbaseband.cpp
    dabdemodsink.cpp
    dabdemodplugin.cpp
    dabdemodwebapiadapter.cpp
)

set(demoddab_HEADERS
    dabdemod.h
    dabdemodsettings.h
    dabdemodbaseband.h
    dabdemodsink.h
    dabdemodplugin.h
    dabdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demoddab_SOURCES
        ${demoddab_SOURCES}
        dabdemodgui.cpp
        dabdemodgui.ui
        dabcarrierplot.cpp
    )
    set(demoddab_HEADERS
        ${demoddab_HEADERS}
        dabdemodgui.h
        dabcarrierplot.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}demoddab)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}demoddabsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${demoddab_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME DABDemodPlugin)
    target_sources(${TARGET_NAME} PRIVATE ${demoddab_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    modemdab
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PROPERTY:${TARGET_NAME},RUNTIME_OUTPUT_DIRECTORY>/${TARGET_NAME}stripped.pdb CONFIGURATIONS Release DESTINATION ${INSTALL_FOLDER} RENAME ${TARGET_NAME}.pdb )
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
