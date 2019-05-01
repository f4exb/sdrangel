project(demodfreedv)

set(freedv_SOURCES
	freedvdemod.cpp
	freedvdemodsettings.cpp
	freedvplugin.cpp
)

set(freedv_HEADERS
	freedvdemod.h
	freedvdemodsettings.h
	freedvplugin.h
)

include_directories(
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
        ${CMAKE_SOURCE_DIR}/libfreedv
        ${CODEC2_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(freedv_SOURCES
        ${freedv_SOURCES}
        freedvdemodgui.cpp

	freedvdemodgui.ui
    )
    set(freedv_HEADERS
        ${freedv_HEADERS}
        freedvdemodgui.h
    )

    set(TARGET_NAME demodfreedv)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodfreedvsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${freedv_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt5::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        freedv
        ${CODEC2_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
