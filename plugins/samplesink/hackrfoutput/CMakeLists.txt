project(hackrfoutput)

set(hackrfoutput_SOURCES
    hackrfoutput.cpp
    hackrfoutputplugin.cpp
    hackrfoutputsettings.cpp
    hackrfoutputwebapiadapter.cpp
    hackrfoutputthread.cpp
)

set(hackrfoutput_HEADERS
    hackrfoutput.h
    hackrfoutputplugin.h
    hackrfoutputsettings.h
    hackrfoutputwebapiadapter.h
    hackrfoutputthread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/devices
    ${LIBHACKRF_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(hackrfoutput_SOURCES
        ${hackrfoutput_SOURCES}
        hackrfoutputgui.cpp

        hackrfoutputgui.ui
    )
    set(hackrfoutput_HEADERS
        ${hackrfoutput_HEADERS}
        hackrfoutputgui.h
    )

    set(TARGET_NAME outputhackrf)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputhackrfsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${hackrfoutput_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBHACKRF_LIBRARIES}
        hackrfdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
