project(localoutput)

set(localoutput_SOURCES
	localoutput.cpp
	localoutputplugin.cpp
    localoutputsettings.cpp
    localoutputwebapiadapter.cpp
)

set(localoutput_HEADERS
	localoutput.h
	localoutputplugin.h
    localoutputsettings.h
    localoutputwebapiadapter.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(localoutput_SOURCES
        ${localoutput_SOURCES}
        localoutputgui.cpp

        localoutputgui.ui
    )
    set(localoutput_HEADERS
        ${localoutput_HEADERS}
        localoutputgui.h
    )

    set(TARGET_NAME outputlocal)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputlocalsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${localoutput_SOURCES}
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
