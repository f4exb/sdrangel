project(radiosonde)

set(radiosonde_SOURCES
    radiosonde.cpp
    radiosondesettings.cpp
    radiosondeplugin.cpp
    radiosondewebapiadapter.cpp
)

set(radiosonde_HEADERS
    radiosonde.h
    radiosondesettings.h
    radiosondeplugin.h
    radiosondewebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(radiosonde_SOURCES
        ${radiosonde_SOURCES}
        radiosondegui.cpp
        radiosondegui.ui
        radiosonde.qrc
    )
    set(radiosonde_HEADERS
        ${radiosonde_HEADERS}
        radiosondegui.h
    )

    set(TARGET_NAME featureradiosonde)
    set(TARGET_LIB Qt::Widgets Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featureradiosondesrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${radiosonde_SOURCES}
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
