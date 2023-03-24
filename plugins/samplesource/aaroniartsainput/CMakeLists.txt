project(aaroniartsainput)

set(aaroniartsainput_SOURCES
	aaroniartsainput.cpp
	aaroniartsainputplugin.cpp
	aaroniartsainputworker.cpp
    aaroniartsainputsettings.cpp
    aaroniartsainputwebapiadapter.cpp
)

set(aaroniartsainput_HEADERS
	aaroniartsainput.h
	aaroniartsainputplugin.h
	aaroniartsainputworker.h
    aaroniartsainputsettings.h
    aaroniartsainputwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(aaroniartsainput_SOURCES
        ${aaroniartsainput_SOURCES}
        aaroniartsainputgui.cpp
        aaroniartsainputgui.ui
    )
    set(aaroniartsainput_HEADERS
        ${aaroniartsainput_HEADERS}
        aaroniartsainputgui.h
    )

	set(TARGET_NAME inputaaroniartsa)
	set(TARGET_LIB "Qt::Widgets")
	set(TARGET_LIB_GUI "sdrgui")
	set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputaaroniartsasrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${aaroniartsainput_SOURCES}
)

target_link_libraries(${TARGET_NAME}
	Qt::Core
	Qt::WebSockets
	${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
