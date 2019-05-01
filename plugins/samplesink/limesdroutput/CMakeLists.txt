project(limesdroutput)

set(limesdroutput_SOURCES
	limesdroutput.cpp
	limesdroutputplugin.cpp
	limesdroutputsettings.cpp
	limesdroutputthread.cpp
)

set(limesdroutput_HEADERS
	limesdroutput.h
	limesdroutputplugin.h
	limesdroutputsettings.h
	limesdroutputthread.h
)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIMESUITE_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(limesdroutput_SOURCES
        ${limesdroutput_SOURCES}
        limesdroutputgui.cpp

        limesdroutputgui.ui
    )
    set(limesdroutput_HEADERS
        ${limesdroutput_HEADERS}
        limesdroutputgui.h
    )

    set(TARGET_NAME outputlimesdr)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputlimesdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${limesdroutput_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIMESUITE_LIBRARY}
        limesdrdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
