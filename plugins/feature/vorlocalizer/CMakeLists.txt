project(vorlocalizer)

set(vor_SOURCES
    vorlocalizer.cpp
    vorlocalizersettings.cpp
    vorlocalizerworker.cpp
    vorlocalizerplugin.cpp
    vorlocalizerwebapiadapter.cpp
    vorlocalizerreport.cpp
)

set(vor_HEADERS
    vorlocalizer.h
    vorlocalizersettings.h
    vorlocalizerworker.h
    vorlocalizerplugin.h
    vorlocalizerwebapiadapter.h
    vorlocalizerreport.h
)


include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(vor_SOURCES
        ${vor_SOURCES}
        vorlocalizergui.cpp
        vorlocalizergui.ui
        map.qrc
        icons.qrc
    )
    set(vor_HEADERS
        ${vor_HEADERS}
        vorlocalizergui.h
    )

    set(TARGET_NAME vorlocalizer)
    set(TARGET_LIB "Qt::Widgets" Qt::Quick Qt::QuickWidgets Qt::Positioning)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME vorlocalizersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${vor_SOURCES}
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
