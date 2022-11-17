project(soapysdrinput)

set(soapysdrinput_SOURCES
	soapysdrinput.cpp
	soapysdrinputplugin.cpp
    soapysdrinputsettings.cpp
    soapysdrinputwebapiadapter.cpp
	soapysdrinputthread.cpp
)

set(soapysdrinput_HEADERS
	soapysdrinput.h
	soapysdrinputplugin.h
    soapysdrinputsettings.h
    soapysdrinputwebapiadapter.h
	soapysdrinputthread.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${SOAPYSDR_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(soapysdrinput_SOURCES
        ${soapysdrinput_SOURCES}
        soapysdrinputgui.cpp

        soapysdrinputgui.ui
    )
    set(soapysdrinput_HEADERS
        ${soapysdrinput_HEADERS}
        soapysdrinputgui.h
    )

    set(TARGET_NAME inputsoapysdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputsoapysdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${soapysdrinput_SOURCES}
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
