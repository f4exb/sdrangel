project(limesdrinput)

set(limesdrinput_SOURCES
	limesdrinput.cpp
	limesdrinputplugin.cpp
    limesdrinputsettings.cpp
    limesdrinputwebapiadapter.cpp
	limesdrinputthread.cpp
)

set(limesdrinput_HEADERS
	limesdrinput.h
	limesdrinputplugin.h
    limesdrinputsettings.h
    limesdrinputwebapiadapter.h
	limesdrinputthread.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIMESUITE_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(limesdrinput_SOURCES
        ${limesdrinput_SOURCES}
        limesdrinputgui.cpp

        limesdrinputgui.ui
    )
    set(limesdrinput_HEADERS
        ${limesdrinput_HEADERS}
        limesdrinputgui.h
    )

    set(TARGET_NAME ${PLUGINS_PREFIX}inputlimesdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}inputlimesdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${limesdrinput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME LimeSDRInputPlugin ${limesdrinput_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if(LIMESUITE_EXTERNAL)
    add_dependencies(${TARGET_NAME} limesuite)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIMESUITE_LIBRARY}
        limesdrdevice
)
if(ANDROID)
    target_link_libraries(${TARGET_NAME} PRIVATE ${LIBUSB_LIBRARIES} log)
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
