project(ais)

set(ais_SOURCES
    ais.cpp
    aissettings.cpp
    aisplugin.cpp
    aiswebapiadapter.cpp
)

set(ais_HEADERS
    ais.h
    aissettings.h
    aisplugin.h
    aiswebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(ais_SOURCES
        ${ais_SOURCES}
        aisgui.cpp
        aisgui.ui
        ais.qrc
    )
    set(ais_HEADERS
        ${ais_HEADERS}
        aisgui.h
    )

    set(TARGET_NAME featureais)
    set(TARGET_LIB Qt::Widgets)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featureaissrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${ais_SOURCES}
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
