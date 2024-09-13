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

    set(TARGET_NAME ${PLUGINS_PREFIX}outputaudio)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME ${PLUGINSSRV_PREFIX}outputaudiosrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

if(NOT Qt6_FOUND)
    add_library(${TARGET_NAME} ${audiooutput_SOURCES})
else()
    qt_add_plugin(${TARGET_NAME} CLASS_NAME AudioOutputPlugin ${audiooutput_SOURCES})
endif()

if(NOT BUILD_SHARED_LIBS)
    set_property(GLOBAL APPEND PROPERTY STATIC_PLUGINS_PROPERTY ${TARGET_NAME})
endif()

if (EMSCRIPTEN AND BUILD_SHARED_LIBS)
    target_link_options(${TARGET_NAME} PUBLIC -sEXPORT_NAME=${TARGET_NAME} -sSIDE_MODULE)
endif()

target_link_libraries(${TARGET_NAME} PRIVATE
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
