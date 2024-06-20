project(wdsprx)

set(wdsprx_SOURCES
	wdsprx.cpp
    wdsprxsettings.cpp
    wdsprxsink.cpp
    wdsprxbaseband.cpp
    wdsprxwebapiadapter.cpp
	wdsprxplugin.cpp
)

set(wdsprx_HEADERS
	wdsprx.h
    wdsprxsettings.h
    wdsprxsink.h
    wdsprxbaseband.h
    wdsprxwebapiadapter.h
	wdsprxplugin.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(wdsprx_SOURCES
        ${wdsprx_SOURCES}
        wdsprxgui.cpp
        wdsprxgui.ui
    )
    set(wdsprx_HEADERS
        ${wdsprx_HEADERS}
        wdsprxgui.h
    )
    set(TARGET_NAME wdsprx)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME wdsprxsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${wdsprx_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
    wdsp
	${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
