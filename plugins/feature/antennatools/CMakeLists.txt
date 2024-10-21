project(antennatools)

set(antennatools_SOURCES
    antennatools.cpp
    antennatoolssettings.cpp
    antennatoolsplugin.cpp
    antennatoolswebapiadapter.cpp
)

set(antennatools_HEADERS
    antennatools.h
    antennatoolssettings.h
    antennatoolsplugin.h
    antennatoolswebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(antennatools_SOURCES
        ${antennatools_SOURCES}
        antennatoolsgui.cpp
        antennatoolsgui.ui
        antennatools.qrc
    )
    set(antennatools_HEADERS
        ${antennatools_HEADERS}
        antennatoolsgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}antennatools)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}antennatoolssrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${antennatools_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AntennaToolsPlugin ${antennatools_SOURCES})
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
