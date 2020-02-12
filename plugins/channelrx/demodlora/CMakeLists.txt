project(lora)

set(lora_SOURCES
	lorademod.cpp
	lorademodgui.cpp
	lorademodsettings.cpp
	lorademodsink.cpp
	lorademodbaseband.cpp
	loraplugin.cpp
	lorademoddecoder.cpp
	lorademodgui.ui
)

set(lora_HEADERS
	lorademod.h
	lorademodgui.h
	lorademodsettings.h
	lorademodsink.h
	lorademodbaseband.h
	lorademoddecoder.h
	loraplugin.h
)

include_directories(
)

add_library(demodlora SHARED
	${lora_SOURCES}
)

target_link_libraries(demodlora
    Qt5::Core
    Qt5::Widgets
	sdrbase
	sdrgui
)

install(TARGETS demodlora DESTINATION ${INSTALL_PLUGINS_DIR})
