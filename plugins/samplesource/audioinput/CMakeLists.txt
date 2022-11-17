project(audioinput)

set(audioinput_SOURCES
    audioinput.cpp
    audioinputplugin.cpp
    audioinputsettings.cpp
    audioinputwebapiadapter.cpp
    audioinputworker.cpp
)

set(audioinput_HEADERS
    audioinput.h
    audioinputplugin.h
    audioinputsettings.h
    audioinputwebapiadapter.h
    audioinputworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(audioinput_SOURCES
        ${audioinput_SOURCES}
        audioinputgui.cpp
        audioinputgui.ui
    )
    set(audioinput_HEADERS
        ${audioinput_HEADERS}
        audioinputgui.h
    )

    set(TARGET_NAME inputaudio)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputaudiosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${audioinput_SOURCES}
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
