project(sdrplay)

set(sdrplay_SOURCES
    sdrplayinput.cpp
    sdrplayplugin.cpp
    sdrplaysettings.cpp
    sdrplaywebapiadapter.cpp
    sdrplaythread.cpp
)

set(sdrplay_HEADERS
    sdrplayinput.h
    sdrplayplugin.h
    sdrplaysettings.h
    sdrplaywebapiadapter.h
    sdrplaythread.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
    ${LIBMIRISDR_INCLUDE_DIR}
)

if(NOT SERVER_MODE)
    set(sdrplay_SOURCES
        ${sdrplay_SOURCES}
        sdrplaygui.cpp

        sdrplaygui.ui
    )
    set(sdrplay_HEADERS
        ${sdrplay_HEADERS}
        sdrplaygui.h
    )

    set(TARGET_NAME inputsdrplay)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputsdrplaysrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${sdrplay_SOURCES}
)

if(LIBMIRISDR_EXTERNAL)
    add_dependencies(${TARGET_NAME} libmirisdr)
endif(LIBMIRISDR_EXTERNAL)

target_link_libraries(${TARGET_NAME}
        Qt::Core
        ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
        swagger
        ${LIBMIRISDR_LIBRARIES}
)

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
