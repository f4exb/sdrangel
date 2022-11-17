project(plutosdroutput)

set(plutosdroutput_SOURCES
    plutosdroutput.cpp
    plutosdroutputplugin.cpp
    plutosdroutputsettings.cpp
    plutosdroutputwebapiadapter.cpp
    plutosdroutputthread.cpp
)

set(plutosdroutput_HEADERS
    plutosdroutput.h
    plutosdroutputplugin.h
    plutosdroutputsettings.h
    plutosdroutputwebapiadapter.h
    plutosdroutputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBIIO_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(plutosdroutput_SOURCES
        ${plutosdroutput_SOURCES}
        plutosdroutputgui.cpp

        plutosdroutputgui.ui
    )
    set(plutosdroutput_HEADERS
        ${plutosdroutput_HEADERS}
        plutosdroutputgui.h
    )

    set(TARGET_NAME outputplutosdr)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputplutosdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${plutosdroutput_SOURCES}
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
