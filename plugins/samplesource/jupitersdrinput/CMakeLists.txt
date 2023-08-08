project(jupitersdrinput)

set(jupitersdrinput_SOURCES
    jupitersdrinput.cpp
    jupitersdrinputplugin.cpp
    jupitersdrinputsettings.cpp
    jupitersdrinputwebapiadapter.cpp
    jupitersdrinputthread.cpp
)

set(jupitersdrinput_HEADERS
    jupitersdrinput.h
    jupitersdrinputplugin.h
    jupitersdrinputsettings.h
    jupitersdrinputwebapiadapter.h
    jupitersdrinputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBIIO_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(jupitersdrinput_SOURCES
        ${jupitersdrinput_SOURCES}
        jupitersdrinputgui.cpp

        jupitersdrinputgui.ui
    )
    set(jupitersdrinput_HEADERS
        ${jupitersdrinput_HEADERS}
        jupitersdrinputgui.h
    )

    set(TARGET_NAME inputjupitersdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputjupitersdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${jupitersdrinput_SOURCES}
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
        jupitersdrdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
