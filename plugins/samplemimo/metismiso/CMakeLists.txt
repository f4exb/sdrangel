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
    set(TARGET_NAME ${PLUGINS_PREFIX}mimometismiso)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}mimometismisosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${metismiso_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME MetisMISOPlugin ${metismiso_SOURCES})
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
    metisdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
