project(fcdpro)

set(fcdpro_SOURCES
	fcdproinput.cpp
	fcdproplugin.cpp
    fcdprosettings.cpp
    fcdprowebapiadapter.cpp
	fcdprothread.cpp
)

set(fcdpro_HEADERS
	fcdproinput.h
	fcdproplugin.h
    fcdprosettings.h
    fcdprowebapiadapter.h
	fcdprothread.h
)

include_directories(
    ${HIDAPI_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${CMAKE_SOURCE_DIR}/fcdhid
	${CMAKE_SOURCE_DIR}/fcdlib
)

if(NOT SERVER_MODE)
    set(fcdpro_SOURCES
        ${fcdpro_SOURCES}
        fcdprogui.cpp
        fcdprogui.ui
    )
    set(fcdpro_HEADERS
        ${fcdpro_HEADERS}
        fcdprogui.h
    )

    set(TARGET_NAME inputfcdpro)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputfcdprosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${fcdpro_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    ${HIDAPI_LIBRARIES}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    fcdhid
    fcdlib
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
