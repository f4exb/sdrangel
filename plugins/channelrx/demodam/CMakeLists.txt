project(am)

set(am_SOURCES
	amdemod.cpp
    amdemodsettings.cpp
    amdemodbaseband.cpp
    amdemodsink.cpp
    amdemodplugin.cpp
    amdemodwebapiadapter.cpp
)

set(am_HEADERS
	amdemod.h
    amdemodsettings.h
    amdemodbaseband.h
    amdemodsink.h
    amdemodplugin.h
    amdemodwebapiadapter.h
)


include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(am_SOURCES
        ${am_SOURCES}
        amdemodgui.cpp
        amdemodssbdialog.cpp
        amdemodgui.ui
        amdemodssb.ui
    )
    set(am_HEADERS
        ${am_HEADERS}
	    amdemodgui.h
        amdemodssbdialog.h
    )

    set(TARGET_NAME demodam)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME demodamsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${am_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
