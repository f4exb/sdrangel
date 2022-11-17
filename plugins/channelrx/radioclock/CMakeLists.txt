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

    set(TARGET_NAME radioclock)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME radioclocksrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${radioclock_SOURCES}
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
