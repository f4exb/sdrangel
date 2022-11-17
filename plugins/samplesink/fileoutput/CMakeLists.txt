project(fileoutput)

set(fileoutput_SOURCES
	fileoutput.cpp
	fileoutputplugin.cpp
	fileoutputsettings.cpp
    fileoutputworker.cpp
    fileoutputwebapiadapter.cpp
)

set(fileoutput_HEADERS
	fileoutput.h
	fileoutputplugin.h
	fileoutputsettings.h
    fileoutputworker.h
    fileoutputwebapiadapter.h
)

include_directories(
        ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(fileoutput_SOURCES
        ${fileoutput_SOURCES}
        fileoutputgui.cpp
        fileoutputgui.ui
    )
    set(fileoutput_HEADERS
        ${fileoutput_HEADERS}
        fileoutputgui.h
    )

    set(TARGET_NAME outputfileoutput)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputfileoutputsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${fileoutput_SOURCES}
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
