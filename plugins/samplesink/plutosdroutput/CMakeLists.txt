project(plutosdroutput)

set(plutosdroutput_SOURCES
    plutosdroutput.cpp
    plutosdroutputplugin.cpp
    plutosdroutputsettings.cpp
    plutosdroutputthread.cpp
)

set(plutosdroutput_HEADERS
    plutosdroutput.h
    plutosdroutputplugin.h
    plutosdroutputsettings.h
    plutosdroutputthread.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
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
    set(TARGET_LIB "Qt5::Widgets")
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

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBIIO_LIBRARIES}
        plutosdrdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
