project(plutosdrinput)

set(plutosdrinput_SOURCES
    plutosdrinput.cpp
    plutosdrinputplugin.cpp
    plutosdrinputsettings.cpp
    plutosdrinputwebapiadapter.cpp
    plutosdrinputthread.cpp
)

set(plutosdrinput_HEADERS
    plutosdrinput.h
    plutosdrinputplugin.h
    plutosdrinputsettings.h
    plutosdrinputwebapiadapter.h
    plutosdrinputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBIIO_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(plutosdrinput_SOURCES
        ${plutosdrinput_SOURCES}
        plutosdrinputgui.cpp

        plutosdrinputgui.ui
    )
    set(plutosdrinput_HEADERS
        ${plutosdrinput_HEADERS}
        plutosdrinputgui.h
    )

    set(TARGET_NAME inputplutosdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputplutosdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${plutosdrinput_SOURCES}
)

if(LIBIIO_EXTERNAL)
    add_dependencies(${TARGET_NAME} libiio)
endif()

target_link_libraries(${TARGET_NAME}
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
