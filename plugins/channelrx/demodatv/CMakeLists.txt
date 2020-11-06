project(atv)

set(atv_SOURCES
    atvdemod.cpp
    atvdemodbaseband.cpp
    atvdemodsink.cpp
    atvdemodsettings.cpp
    atvdemodwebapiadapter.cpp
	atvdemodgui.cpp
	atvdemodplugin.cpp
	atvdemodgui.ui
)

set(atv_HEADERS
    atvdemod.h
    atvdemodbaseband.h
    atvdemodsink.h
    atvdemodsettings.h
    atvdemodwebapiadapter.h
	atvdemodgui.h
	atvdemodplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
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
