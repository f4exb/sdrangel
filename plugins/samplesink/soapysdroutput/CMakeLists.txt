project(soapysdroutput)

set(soapysdroutput_SOURCES
	soapysdroutput.cpp
	soapysdroutputplugin.cpp
    soapysdroutputsettings.cpp
    soapysdroutputwebapiadapter.cpp
	soapysdroutputthread.cpp
)

set(soapysdroutput_HEADERS
	soapysdroutput.h
	soapysdroutputplugin.h
    soapysdroutputsettings.h
    soapysdroutputwebapiadapter.h
	soapysdroutputthread.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${SOAPYSDR_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(soapysdroutput_SOURCES
        ${soapysdroutput_SOURCES}
        soapysdroutputgui.cpp

        soapysdroutputgui.ui
    )
    set(soapysdroutput_HEADERS
        ${soapysdroutput_HEADERS}
        soapysdroutputgui.h
    )

    set(TARGET_NAME outputsoapysdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputsoapysdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${soapysdroutput_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${SOAPYSDR_LIBRARY}
        soapysdrdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
