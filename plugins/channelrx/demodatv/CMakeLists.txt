project(atv)

set(atv_SOURCES
	atvdemod.cpp
	atvdemodsettings.cpp
	atvdemodgui.cpp
	atvdemodplugin.cpp
	atvscreen.cpp
	glshaderarray.cpp
)

set(atv_HEADERS
	atvdemod.h
    atvdemodsettings.h
	atvdemodgui.h
	atvdemodplugin.h
	atvscreen.h
	atvscreeninterface.h
	glshaderarray.h
)

set(atv_FORMS
	atvdemodgui.ui
)

include_directories(
	.
	${CMAKE_CURRENT_BINARY_DIR}
)

#include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

#qt5_wrap_cpp(nfm_HEADERS_MOC ${nfm_HEADERS})
qt5_wrap_ui(atv_FORMS_HEADERS ${atv_FORMS})

add_library(demodatv SHARED
	${atv_SOURCES}
	${atv_HEADERS_MOC}
	${atv_FORMS_HEADERS}
)

target_link_libraries(demodatv
	${QT_LIBRARIES}
	sdrbase
	sdrgui
)

qt5_use_modules(demodatv Core Widgets)

install(TARGETS demodatv DESTINATION lib/plugins/channelrx)
