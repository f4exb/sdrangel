project(lora)

set(lora_SOURCES
	lorademod.cpp
	lorademodgui.cpp
	lorademodsettings.cpp
	lorademodsink.cpp
	lorademodbaseband.cpp
	loraplugin.cpp
	lorademoddecoder.cpp
	lorademoddecodertty.cpp
    lorademoddecoderascii.cpp
    lorademoddecoderlora.cpp
	lorademodmsg.cpp
	lorademodgui.ui
)

set(lora_HEADERS
	lorademod.h
	lorademodgui.h
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

add_library(demodlora SHARED
	${lora_SOURCES}
)

target_link_libraries(demodlora
    Qt5::Core
    Qt5::Widgets
	sdrbase
    sdrgui
    swagger
)

install(TARGETS demodlora DESTINATION ${INSTALL_PLUGINS_DIR})
