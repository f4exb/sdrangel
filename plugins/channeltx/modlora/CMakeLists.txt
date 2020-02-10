project(modlora)

set(modlora_SOURCES
	loramod.cpp
	loramodsettings.cpp
	loramodsource.cpp
	loramodbaseband.cpp
    loramodplugin.cpp
    loramodwebapiadapter.cpp
)

set(modlora_HEADERS
	loramod.h
	loramodsettings.h
	loramodsource.h
	loramodbaseband.h
    loramodplugin.h
    loramodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modlora_SOURCES
        ${modlora_SOURCES}
        loramodgui.cpp
        loramodgui.ui
    )
    set(modlora_HEADERS
        ${modlora_HEADERS}
    	loramodgui.h
    )

    set(TARGET_NAME modlora)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modlorasrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${modlora_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt5::Core
    ${TARGET_LIB}
    sdrbase
    ${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
