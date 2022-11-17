project(vormc)

set(vormc_SOURCES
    vordemodmc.cpp
    vordemodmcsettings.cpp
    vordemodmcbaseband.cpp
    vordemodmcsink.cpp
    vordemodmcplugin.cpp
    vordemodmcwebapiadapter.cpp
    vordemodmcreport.cpp
)

set(vormc_HEADERS
    vordemodmc.h
    vordemodmcsettings.h
    vordemodmcbaseband.h
    vordemodmcsink.h
    vordemodmcplugin.h
    vordemodmcwebapiadapter.h
    vordemodmcreport.h
)


include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(vormc_SOURCES
        ${vormc_SOURCES}
        vordemodmcgui.cpp
        vordemodmcgui.ui
        map.qrc
        icons.qrc
    )
    set(vormc_HEADERS
        ${vormc_HEADERS}
        vordemodmcgui.h
        navaid.h
        ../demodadsb/csv.h
    )

    set(TARGET_NAME demodvormc)
    set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Positioning)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodvormcsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${vormc_SOURCES}
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
