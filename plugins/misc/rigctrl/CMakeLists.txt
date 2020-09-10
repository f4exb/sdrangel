project(rigctrl)

set(rigctrl_SOURCES
    rigctrl.cpp
    rigctrlsettings.cpp
    rigctrlplugin.cpp
)

set(rigctrl_HEADERS
    rigctrl.h
    rigctrlsettings.h
    rigctrlplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(rigctrl_SOURCES
        ${rigctrl_SOURCES}
        rigctrlgui.cpp
        rigctrlgui.ui
    )
    set(rigctrl_HEADERS
        ${rigctrl_HEADERS}
        rigctrlgui.h
    )
    set(TARGET_NAME rigctrl)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME rigctrlsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${rigctrl_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt5::Core
    ${TARGET_LIB}
	sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
