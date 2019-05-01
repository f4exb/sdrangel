project(lora)

set(lora_SOURCES
	lorademod.cpp
	lorademodgui.cpp
	lorademodsettings.cpp
	loraplugin.cpp

	lorademodgui.ui
)

set(lora_HEADERS
	lorademod.h
	lorademodgui.h
	lorademodsettings.h
	loraplugin.h
)

include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
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

install(TARGETS demodlora DESTINATION ${INSTALL_LIB_DIR}/plugins/channelrx)
