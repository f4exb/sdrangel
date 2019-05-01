project(xtrxinput)

set(xtrxinput_SOURCES
	xtrxinput.cpp
	xtrxinputplugin.cpp
	xtrxinputsettings.cpp
	xtrxinputthread.cpp
)

set(xtrxinput_HEADERS
	xtrxinput.h
	xtrxinputplugin.h
	xtrxinputsettings.h
	xtrxinputthread.h
)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBXTRX_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(xtrxinput_SOURCES
        ${xtrxinput_SOURCES}
        xtrxinputgui.cpp

        xtrxinputgui.ui
    )
    set(xtrxinput_HEADERS
        ${xtrxinput_HEADERS}
        xtrxinputgui.h
    )

    set(TARGET_NAME inputxtrx)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER "plugins")
else()
    set(TARGET_NAME inputxtrxsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER "pluginssrv")
endif()

add_library(${TARGET_NAME} SHARED
	${xtrxinput_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBXTRX_LIBRARIES}
        xtrxdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_LIB_DIR}/${INSTALL_FOLDER}/samplesource)
