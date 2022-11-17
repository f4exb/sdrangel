project(xtrxmimo)

set(xtrxmimo_SOURCES
    xtrxmimo.cpp
	xtrxmimoplugin.cpp
	xtrxmithread.cpp
	xtrxmothread.cpp
    xtrxmimosettings.cpp
    xtrxmimowebapiadapter.cpp
)

set(xtrxmimo_HEADERS
    xtrxmimo.h
	xtrxmimoplugin.h
	xtrxmithread.h
	xtrxmothread.h
    xtrxmimosettings.h
    xtrxmimowebapiadapter.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
	${CMAKE_SOURCE_DIR}/devices
	${LIBXTRX_INCLUDE_DIRS}
)

if (NOT SERVER_MODE)
    set (xtrxmimo_SOURCES
        ${xtrxmimo_SOURCES}
        xtrxmimogui.cpp
        xtrxmimogui.ui
    )
    set(xtrxmimo_HEADERS
        ${xtrxmimo_HEADERS}
        xtrxmimogui.h
    )
    set(TARGET_NAME mimoxtrx)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME mimoxtrxsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${xtrxmimo_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
    ${LIBXTRX_LIBRARY}
    xtrxdevice
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
