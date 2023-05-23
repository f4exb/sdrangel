project(aaroniartsaoutput)

set(aaroniartsaoutput_SOURCES
	aaroniartsaoutput.cpp
	aaroniartsaoutputplugin.cpp
    aaroniartsaoutputsettings.cpp
    aaroniartsaoutputwebapiadapter.cpp
    aaroniartsaoutputworker.cpp
)

set(aaroniartsaoutput_HEADERS
	aaroniartsaoutput.h
	aaroniartsaoutputplugin.h
    aaroniartsaoutputsettings.h
    aaroniartsaoutputwebapiadapter.h
    aaroniartsaoutputworker.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(aaroniartsaoutput_SOURCES
        ${aaroniartsaoutput_SOURCES}
        aaroniartsaoutputgui.cpp
        aaroniartsaoutputgui.ui
    )
    set(aaroniartsaoutput_HEADERS
        ${aaroniartsaoutput_HEADERS}
        aaroniartsaoutputgui.h
    )

    set(TARGET_NAME outputaaroniartsa)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputaaroniartsasrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${aaroniartsaoutput_SOURCES}
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
