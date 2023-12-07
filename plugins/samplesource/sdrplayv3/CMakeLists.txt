project(sdrplay)

set(sdrplayv3_SOURCES
    sdrplayv3input.cpp
    sdrplayv3plugin.cpp
    sdrplayv3settings.cpp
    sdrplayv3webapiadapter.cpp
    sdrplayv3thread.cpp
)

set(sdrplayv3_HEADERS
    sdrplayv3input.h
    sdrplayv3plugin.h
    sdrplayv3settings.h
    sdrplayv3webapiadapter.h
    sdrplayv3thread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${SDRPLAY_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(sdrplayv3_SOURCES
        ${sdrplayv3_SOURCES}
        sdrplayv3gui.cpp
        sdrplayv3gui.ui
    )
    set(sdrplayv3_HEADERS
        ${sdrplayv3_HEADERS}
        sdrplayv3gui.h
    )

    set(TARGET_NAME inputsdrplayv3)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputsdrplayv3srv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${sdrplayv3_SOURCES}
)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${SDRPLAY_LIBRARIES}
)

#  /usr/local/lib doesn't seem to be searched by default
if (APPLE AND (CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL arm64))
    add_custom_command(TARGET ${TARGET_NAME}
        POST_BUILD COMMAND
        ${CMAKE_INSTALL_NAME_TOOL} -change libsdrplay_api.so.3.12 /usr/local/lib/libsdrplay_api.so.3.12  $<TARGET_FILE:${TARGET_NAME}>)
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
