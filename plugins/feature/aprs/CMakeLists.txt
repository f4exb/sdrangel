project(aprs)

set(aprs_SOURCES
    aprs.cpp
    aprssettings.cpp
    aprsplugin.cpp
    aprsworker.cpp
    aprswebapiadapter.cpp
)

set(aprs_HEADERS
    aprs.h
    aprssettings.h
    aprsplugin.h
    aprsreport.h
    aprsworker.h
    aprswebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(aprs_SOURCES
        ${aprs_SOURCES}
        aprsgui.cpp
        aprsgui.ui
        aprssettingsdialog.cpp
        aprssettingsdialog.ui
        aprs.qrc
    )
    set(aprs_HEADERS
        ${aprs_HEADERS}
        aprsgui.h
        aprssettingsdialog.h
    )

    set(TARGET_NAME aprs)
    set(TARGET_LIB "Qt::Widgets" Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME aprssrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${aprs_SOURCES}
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
