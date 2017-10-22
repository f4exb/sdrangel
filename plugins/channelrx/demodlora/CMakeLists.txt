project(lora)

set(lora_SOURCES
	lorademod.cpp
	lorademodgui.cpp
	lorademodsettings.cpp
	loraplugin.cpp
)

set(lora_HEADERS
	lorademod.h
	lorademodgui.h
	lorademodsettings.h
	loraplugin.h
)

set(lora_FORMS
	lorademodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(lora_HEADERS_MOC ${lora_HEADERS})
qt5_wrap_ui(lora_FORMS_HEADERS ${lora_FORMS})

add_library(demodlora SHARED
	${lora_SOURCES}
	${lora_HEADERS_MOC}
	${lora_FORMS_HEADERS}
)

target_link_libraries(demodlora
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(demodlora Core Widgets)

install(TARGETS demodlora DESTINATION lib/plugins/channelrx)
