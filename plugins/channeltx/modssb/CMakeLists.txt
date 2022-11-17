project(modssb)

set(modssb_SOURCES
    ssbmod.cpp
    ssbmodbaseband.cpp
    ssbmodsource.cpp
    ssbmodplugin.cpp
    ssbmodsettings.cpp
    ssbmodwebapiadapter.cpp
)

set(modssb_HEADERS
    ssbmod.h
    ssbmodbaseband.h
    ssbmodsource.h
    ssbmodplugin.h
    ssbmodsettings.h
    ssbmodwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modssb_SOURCES
        ${modssb_SOURCES}
        ssbmodgui.cpp

        ssbmodgui.ui
    )
    set(modssb_HEADERS
        ${modssb_HEADERS}
        ssbmodgui.h
    )

    set(TARGET_NAME modssb)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modssbsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${modssb_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
