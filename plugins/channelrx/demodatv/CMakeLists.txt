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

set(TARGET_NAME demodatv)
set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})

add_library(${TARGET_NAME} SHARED
	${atv_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    Qt::Widgets
	sdrbase
	sdrgui
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
