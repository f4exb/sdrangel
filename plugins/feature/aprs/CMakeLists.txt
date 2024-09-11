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

    set(TARGET_NAME ${PLUGINS_PREFIX}aprs)
    set(TARGET_LIB "Qt::Widgets" Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}aprssrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${aprs_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME APRSPlugin ${aprs_SOURCES})
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
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
