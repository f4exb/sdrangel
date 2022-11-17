project(jogdialcontroller)

set(jogdialcontroller_SOURCES
    jogdialcontroller.cpp
    jogdialcontrollersettings.cpp
    jogdialcontrollerplugin.cpp
    jogdialcontrollerwebapiadapter.cpp
)

set(jogdialcontroller_HEADERS
    jogdialcontroller.h
    jogdialcontrollersettings.h
    jogdialcontrollerplugin.h
    jogdialcontrollerwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(jogdialcontroller_SOURCES
        ${jogdialcontroller_SOURCES}
        jogdialcontrollergui.cpp
        jogdialcontrollergui.ui
    )
    set(jogdialcontroller_HEADERS
        ${jogdialcontroller_HEADERS}
        jogdialcontrollergui.h
    )

    set(TARGET_NAME jogdialcontroller)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME jogdialcontrollersrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${jogdialcontroller_SOURCES}
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
