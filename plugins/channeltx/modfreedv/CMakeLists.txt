project(modfreedv)

set(modfreedv_SOURCES
    freedvmod.cpp
    freedvmodplugin.cpp
    freedvmodsettings.cpp
)

set(modfreedv_HEADERS
    freedvmod.h
    freedvmodplugin.h
    freedvmodsettings.h
)

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/libfreedv
    ${CODEC2_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(modfreedv_SOURCES
        ${modfreedv_SOURCES}
        freedvmodgui.cpp

        freedvmodgui.ui
    )
    set(modfreedv_HEADERS
        ${modfreedv_HEADERS}
        freedvmodgui.h
    )

    set(TARGET_NAME modfreedv)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modfreedvsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${modfreedv_SOURCES}
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
