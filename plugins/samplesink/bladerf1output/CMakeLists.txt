project(bladerf1output)

set(bladerf1output_SOURCES
	bladerf1output.cpp
	bladerf1outputplugin.cpp
    bladerf1outputsettings.cpp
    bladerf1outputwebapiadapter.cpp
	bladerf1outputthread.cpp
)

set(bladerf1output_HEADERS
	bladerf1output.h
	bladerf1soutputplugin.h
    bladerf1outputsettings.h
    bladerf1outputwebapiadapter.h
	bladerf1outputthread.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBBLADERF_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(bladerf1output_SOURCES
        ${bladerf1output_SOURCES}
        bladerf1outputgui.cpp

        bladerf1outputgui.ui
    )
    set(bladerf1output_HEADERS
        ${bladerf1output_HEADERS}
        bladerf1outputgui.h
    )

    set(TARGET_NAME outputbladerf1)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputbladerf1srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${bladerf1output_SOURCES}
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
        bladerf1device
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
