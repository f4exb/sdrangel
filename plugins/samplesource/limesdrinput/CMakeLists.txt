project(limesdrinput)

set(limesdrinput_SOURCES
	limesdrinput.cpp
	limesdrinputplugin.cpp
	limesdrinputsettings.cpp
	limesdrinputthread.cpp
)

set(limesdrinput_HEADERS
	limesdrinput.h
	limesdrinputplugin.h
	limesdrinputsettings.h
	limesdrinputthread.h
)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
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

    set(TARGET_NAME inputlimesdr)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER "plugins")
else()
    set(TARGET_NAME inputlimesdrsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER "pluginssrv")
endif()

add_library(${TARGET_NAME} SHARED
	${limesdrinput_SOURCES}
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

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_LIB_DIR}/${INSTALL_FOLDER}/samplesource)
