project(spectranmiso)

set(spectranmiso_SOURCES
	spectranmiso.cpp
	spectranmisoplugin.cpp
    spectranmisosettings.cpp
    spectranmisowebapiadapter.cpp
    spectranmisostreamworker.cpp
)

set(spectranmiso_HEADERS
	spectranmiso.h
	spectranmisoplugin.h
    spectranmisosettings.h
    spectranmisowebapiadapter.h
    spectranmisostreamworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${RTSAAPI_INCLUDE_DIR}
)

if (NOT SERVER_MODE)
    set (spectranmiso_SOURCES
        ${spectranmiso_SOURCES}
        spectranmisogui.cpp
        spectranmisogui.ui
    )
    set(spectranmiso_HEADERS
        ${spectranmiso_HEADERS}
        spectranmisogui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}spectranmiso)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}spectranmisosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${spectranmiso_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME SpectranMISOPlugin ${spectranmiso_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    spectrandevice
    ${RTSAAPI_LIBRARY}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
