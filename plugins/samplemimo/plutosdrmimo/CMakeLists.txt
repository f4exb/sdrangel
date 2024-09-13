project(plutosdrmimo)

set(plutosdrmimo_SOURCES
    plutosdrmimo.cpp
	plutosdrmimoplugin.cpp
	plutosdrmithread.cpp
	plutosdrmothread.cpp
    plutosdrmimosettings.cpp
    plutosdrmimowebapiadapter.cpp
)

set(plutosdrmimo_HEADERS
    plutosdrmimo.h
	plutosdrmimoplugin.h
	plutosdrmithread.h
	plutosdrmothread.h
    plutosdrmimosettings.h
    plutosdrmimowebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBIIO_INCLUDE_DIR}
)

if (NOT SERVER_MODE)
    set (plutosdrmimo_SOURCES
        ${plutosdrmimo_SOURCES}
        plutosdrmimogui.cpp
        plutosdrmimogui.ui
    )
    set(plutosdrmimo_HEADERS
        ${plutosdrmimo_HEADERS}
        plutosdrmimogui.h
    )
    set(TARGET_NAME ${PLUGINS_PREFIX}mimoplutosdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}mimoplutosdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${plutosdrmimo_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME PlutoSDRMIMOPlugin ${plutosdrmimo_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(LIBIIO_EXTERNAL)
    add_dependencies(${TARGET_NAME} libiio)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${LIBIIO_LIBRARIES}
    plutosdrdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
