project(demoddsc)

set(demoddsc_SOURCES
    dscdemod.cpp
    dscdemodsettings.cpp
    dscdemodbaseband.cpp
    dscdemodsink.cpp
    dscdemodplugin.cpp
    dscdemodwebapiadapter.cpp
)

set(demoddsc_HEADERS
    dscdemod.h
    dscdemodsettings.h
    dscdemodbaseband.h
    dscdemodsink.h
    dscdemodplugin.h
    dscdemodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(demoddsc_SOURCES
        ${demoddsc_SOURCES}
        dscdemodgui.cpp
        dscdemodgui.ui
    )
    set(demoddsc_HEADERS
        ${demoddsc_HEADERS}
        dscdemodgui.h
    )

    set(TARGET_NAME demoddsc)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demoddscsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${demoddsc_SOURCES}
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
