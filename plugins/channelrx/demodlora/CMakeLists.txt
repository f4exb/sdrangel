project(lora)

set(lora_SOURCES
	lorademod.cpp
	lorademodsettings.cpp
	lorademodsink.cpp
	lorademodbaseband.cpp
	loraplugin.cpp
	lorademoddecoder.cpp
	lorademoddecodertty.cpp
    lorademoddecoderascii.cpp
    lorademoddecoderlora.cpp
	lorademodmsg.cpp
)

set(lora_HEADERS
	lorademod.h
	lorademodsettings.h
	lorademodsink.h
	lorademodbaseband.h
	lorademoddecoder.h
	lorademoddecodertty.h
    lorademoddecoderascii.h
    lorademoddecoderlora.h
	lorademodmsg.h
	loraplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(lora_SOURCES
        ${lora_SOURCES}
        lorademodgui.cpp
        lorademodgui.ui
    )
    set(lora_HEADERS
        ${lora_HEADERS}
        lorademodgui.h
    )
    set(TARGET_NAME demodlora)
    set(TARGET_LIB "Qt5::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodlorasrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${lora_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt5::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})
