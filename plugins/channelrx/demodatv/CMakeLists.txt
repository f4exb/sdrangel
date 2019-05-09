project(atv)

set(atv_SOURCES
	atvdemod.cpp
	atvdemodsettings.cpp
	atvdemodgui.cpp
	atvdemodplugin.cpp

	atvdemodgui.ui
)

set(atv_HEADERS
	atvdemod.h
        atvdemodsettings.h
	atvdemodgui.h
	atvdemodplugin.h
)

include_directories(
	${Boost_INCLUDE_DIRS}
)

add_library(demodatv SHARED
	${atv_SOURCES}
)

target_link_libraries(demodatv
        Qt5::Core
        Qt5::Widgets
	sdrbase
	sdrgui
)

install(TARGETS demodatv DESTINATION ${INSTALL_PLUGINS_DIR})
