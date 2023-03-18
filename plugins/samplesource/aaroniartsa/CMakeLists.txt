project(aaroniartsa)

set(aaroniartsa_SOURCES
	aaroniartsainput.cpp
	aaroniartsaplugin.cpp
	aaroniartsaworker.cpp
    aaroniartsasettings.cpp
    aaroniartsawebapiadapter.cpp
)

set(aaroniartsa_HEADERS
	aaroniartsainput.h
	aaroniartsaplugin.h
	aaroniartsaworker.h
    aaroniartsasettings.h
    aaroniartsawebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${Boost_INCLUDE_DIRS}
)

if(NOT SERVER_MODE)
    set(aaroniartsa_SOURCES
        ${aaroniartsa_SOURCES}
        aaroniartsagui.cpp
        aaroniartsagui.ui
    )
    set(aaroniartsa_HEADERS
        ${aaroniartsa_HEADERS}
        aaroniartsagui.h
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
	${aaroniartsa_SOURCES}
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
