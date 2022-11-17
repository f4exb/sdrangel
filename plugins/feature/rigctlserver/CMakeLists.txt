project(rigctlserver)

set(rigctlserver_SOURCES
	rigctlserver.cpp
    rigctlserversettings.cpp
    rigctlserverplugin.cpp
    rigctlserverworker.cpp
    rigctlserverwebapiadapter.cpp
)

set(rigctlserver_HEADERS
	rigctlserver.h
    rigctlserversettings.h
    rigctlserverplugin.h
    rigctlserverworker.h
    rigctlserverwebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(rigctlserver_SOURCES
        ${rigctlserver_SOURCES}
        rigctlservergui.cpp
        rigctlservergui.ui
    )
    set(rigctlserver_HEADERS
        ${rigctlserver_HEADERS}
	    rigctlservergui.h
    )

    set(TARGET_NAME featurerigctlserver)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME featurerigctlserversrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${rigctlserver_SOURCES}
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
