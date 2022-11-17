project(metismiso)

set(metismiso_SOURCES
	metismiso.cpp
	metismisoplugin.cpp
	metismisoudphandler.cpp
    metismisosettings.cpp
    metismisowebapiadapter.cpp
    metismisodecimators.cpp
)

set(metismiso_HEADERS
	metismiso.h
	metismisoplugin.h
	metismisoudphandler.h
    metismisosettings.h
    metismisowebapiadapter.h
    metismisodecimators.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
)

if (NOT SERVER_MODE)
    set (metismiso_SOURCES
        ${metismiso_SOURCES}
        metismisogui.cpp
        metismisogui.ui
    )
    set(metismiso_HEADERS
        ${metismiso_HEADERS}
        metismisogui.h
    )
    set(TARGET_NAME mimometismiso)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME mimometismisosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${metismiso_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    metisdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
