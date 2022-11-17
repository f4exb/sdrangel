project(modam)

set(modam_SOURCES
	ammod.cpp
	ammodplugin.cpp
    ammodsettings.cpp
    ammodbaseband.cpp
    ammodsource.cpp
    ammodwebapiadapter.cpp
)

set(modam_HEADERS
	ammod.h
	ammodplugin.h
    ammodsettings.h
    ammodbaseband.h
    ammodsource.h
    ammodwebapiadapter.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(modam_SOURCES
        ${modam_SOURCES}
        ammodgui.cpp

        ammodgui.ui
    )
    set(modam_HEADERS
        ${modam_HEADERS}
        ammodgui.h
    )

    set(TARGET_NAME modam)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME modamsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${modam_SOURCES}
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
