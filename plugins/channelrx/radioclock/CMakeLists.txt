project(radioclock)

set(radioclock_SOURCES
    radioclock.cpp
    radioclocksettings.cpp
    radioclockbaseband.cpp
    radioclocksink.cpp
    radioclockplugin.cpp
    radioclockwebapiadapter.cpp
)

set(radioclock_HEADERS
    radioclock.h
    radioclocksettings.h
    radioclockbaseband.h
    radioclocksink.h
    radioclockplugin.h
    radioclockwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(radioclock_SOURCES
        ${radioclock_SOURCES}
        radioclockgui.cpp
        radioclockgui.ui
    )
    set(radioclock_HEADERS
        ${radioclock_HEADERS}
        radioclockgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}radioclock)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}radioclocksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${radioclock_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME RadioClockPlugin ${radioclock_SOURCES})
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
