project(radioastronomy)

set(radioastronomy_SOURCES
    radioastronomy.cpp
    radioastronomysettings.cpp
    radioastronomybaseband.cpp
    radioastronomysink.cpp
    radioastronomyplugin.cpp
    radioastronomywebapiadapter.cpp
    radioastronomyworker.cpp
)

set(radioastronomy_HEADERS
    radioastronomy.h
    radioastronomysettings.h
    radioastronomybaseband.h
    radioastronomysink.h
    radioastronomyplugin.h
    radioastronomywebapiadapter.h
    radioastronomyworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(radioastronomy_SOURCES
        ${radioastronomy_SOURCES}
        radioastronomygui.cpp
        radioastronomygui.ui
        radioastronomycalibrationdialog.cpp
        radioastronomycalibrationdialog.ui
        radioastronomysensordialog.cpp
        radioastronomysensordialog.ui
        radioastronomyicons.qrc
    )
    set(radioastronomy_HEADERS
        ${radioastronomy_HEADERS}
        radioastronomygui.h
        radioastronomycalibrationdialog.h
        radioastronomysensordialog.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}radioastronomy)
    set(TARGET_LIB "Qt::Widgets" Qt::Charts)
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}radioastronomysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${radioastronomy_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RadioAstronomyPlugin ${radioastronomy_SOURCES})
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
