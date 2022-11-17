project(modm17)

set(modm17_SOURCES
    m17mod.cpp
    m17modbaseband.cpp
    m17modsource.cpp
    m17modprocessor.cpp
    m17modfifo.cpp
    m17moddecimator.cpp
    m17modax25.cpp
	m17modplugin.cpp
    m17modsettings.cpp
    m17modwebapiadapter.cpp
)

set(modm17_HEADERS
	m17mod.h
    m17modbaseband.h
    m17modsource.h
    m17modprocessor.h
    m17modfifo.h
    m17moddecimator.h
    m17modax25.h
	m17modplugin.h
    m17modsettings.h
    m17modwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CODEC2_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/modemm17
)

if(NOT SERVER_MODE)
    set(modm17_SOURCES
        ${modm17_SOURCES}
        m17modgui.cpp
        m17modgui.ui
    )
    set(modm17_HEADERS
        ${modm17_HEADERS}
        m17modgui.h
    )

    set(TARGET_NAME modm17)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modm17srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${modm17_SOURCES}
)

if(CODEC2_EXTERNAL)
    add_dependencies(${TARGET_NAME} codec2)
endif()

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${CODEC2_LIBRARIES}
    modemm17
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
