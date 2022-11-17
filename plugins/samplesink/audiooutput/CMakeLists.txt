project(audiooutput)

set(audiooutput_SOURCES
    audiooutput.cpp
    audiooutputplugin.cpp
    audiooutputsettings.cpp
    audiooutputwebapiadapter.cpp
    audiooutputworker.cpp
)

set(audiooutput_HEADERS
    audiooutput.h
    audiooutputplugin.h
    audiooutputsettings.h
    audiooutputwebapiadapter.h
    audiooutputworker.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(audiooutput_SOURCES
        ${audiooutput_SOURCES}
        audiooutputgui.cpp
        audiooutputgui.ui
    )
    set(audiooutput_HEADERS
        ${audiooutput_HEADERS}
        audiooutputgui.h
    )

    set(TARGET_NAME outputaudio)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME outputaudiosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
    ${audiooutput_SOURCES}
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
