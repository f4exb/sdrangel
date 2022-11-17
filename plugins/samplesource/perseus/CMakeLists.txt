project(perseus)

set(perseus_SOURCES
    perseusinput.cpp
    perseusplugin.cpp
    perseussettings.cpp
    perseuswebapiadapter.cpp
    perseusworker.cpp
)

set(perseus_HEADERS
    perseusinput.h
    perseusplugin.h
    perseussettings.h
    perseuswebapiadapter.h
    perseusworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBUSB_INCLUDE_DIR}
    ${LIBPERSEUS_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(perseus_SOURCES
        ${perseus_SOURCES}
        perseusgui.cpp

        perseusgui.ui
    )
    set(perseus_HEADERS
        ${perseus_HEADERS}
        perseusgui.h
    )

    set(TARGET_NAME inputperseus)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputperseussrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${perseus_SOURCES}
)

if(LIBPERSEUS_EXTERNAL)
    add_dependencies(${TARGET_NAME} perseus)
endif()

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBUSB_LIBRARIES}
        ${LIBPERSEUS_LIBRARIES}
        perseusdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
