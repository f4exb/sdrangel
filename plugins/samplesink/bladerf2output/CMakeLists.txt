project(bladerf2output)

set(bladerf2output_SOURCES
	bladerf2output.cpp
	bladerf2outputplugin.cpp
    bladerf2outputsettings.cpp
    bladerf2outputwebapiadapter.cpp
	bladerf2outputthread.cpp
)

set(bladerf2output_HEADERS
	bladerf2output.h
	bladerf2outputplugin.h
    bladerf2outputsettings.h
    bladerf2outputwebapiadapter.h
	bladerf2outputthread.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBBLADERF_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(bladerf2output_SOURCES
        ${bladerf2output_SOURCES}
        bladerf2outputgui.cpp

        bladerf2outputgui.ui
    )
    set(bladerf2output_HEADERS
        ${bladerf2output_HEADERS}
        bladerf2outputgui.h
    )

    set(TARGET_NAME outputbladerf2)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputbladerf2srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${bladerf2output_SOURCES}
)

if(LIBBLADERF_EXTERNAL)
    add_dependencies(${TARGET_NAME} bladerf)
endif()

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBBLADERF_LIBRARIES}
        bladerf2device
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
