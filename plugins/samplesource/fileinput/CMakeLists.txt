project(fileinput)

set(fileinput_SOURCES
	fileinput.cpp
	fileinputplugin.cpp
	fileinputworker.cpp
    fileinputsettings.cpp
    fileinputwebapiadapter.cpp
)

set(fileinput_HEADERS
	fileinput.h
	fileinputplugin.h
	fileinputworker.h
    fileinputsettings.h
    fileinputwebapiadapter.h
)

include_directories(
	${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(fileinput_SOURCES
        ${fileinput_SOURCES}
        fileinputgui.cpp
        fileinputgui.ui
    )
    set(fileinput_HEADERS
        ${fileinput_HEADERS}
        fileinputgui.h
    )
    set(TARGET_NAME inputfileinput)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputfileinputsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${fileinput_SOURCES}
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
