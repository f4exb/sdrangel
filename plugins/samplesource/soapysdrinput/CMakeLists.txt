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
	${SOAPYSDR_INCLUDE_DIRS}
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

    set(TARGET_NAME ${PLUGINS_PREFIX}inputsoapysdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputsoapysdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${soapysdrinput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME SoapySDRInputPlugin ${soapysdrinput_SOURCES})
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
        ${SOAPYSDR_LINK_LIBRARIES}
        ${SOAPYSDR_LIBRARIES}
        soapysdrdevice
)

target_link_directories(${TARGET_NAME} PRIVATE
    ${SOAPYSDR_LIBRARY_DIRS}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
