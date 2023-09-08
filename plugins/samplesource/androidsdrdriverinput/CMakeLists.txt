project(androidsdrdriverinput)

set(androidsdrdriverinput_SOURCES
    androidsdrdriverinputtcphandler.cpp
    androidsdrdriverinput.cpp
    androidsdrdriverinputsettings.cpp
    androidsdrdriverinputwebapiadapter.cpp
    androidsdrdriverinputplugin.cpp
)

set(androidsdrdriverinput_HEADERS
    androidsdrdriverinputtcphandler.h
    androidsdrdriverinput.h
    androidsdrdriverinputsettings.h
    androidsdrdriverinputwebapiadapter.h
    androidsdrdriverinputplugin.h
)

include_directories(
    ${CMAKE_SOURCE_DIR}/swagger/sdrangel/code/qt5/client
)

if(NOT SERVER_MODE)
    set(androidsdrdriverinput_SOURCES
        ${androidsdrdriverinput_SOURCES}
        androidsdrdriverinputgui.cpp
        androidsdrdriverinputgui.ui
    )
    set(androidsdrdriverinput_HEADERS
        ${androidsdrdriverinput_HEADERS}
        androidsdrdriverinputgui.h
    )

    set(TARGET_NAME inputandroidsdrdriverinput)
    set(TARGET_LIB "Qt::Widgets")
    set(TARGET_LIB_GUI "sdrgui")
    set(INSTALL_FOLDER ${INSTALL_PLUGINS_DIR})
else()
    set(TARGET_NAME inputandroidsdrdriverinputsrv)
    set(TARGET_LIB "")
    set(TARGET_LIB_GUI "")
    set(INSTALL_FOLDER ${INSTALL_PLUGINSSRV_DIR})
endif()

add_library(${TARGET_NAME} SHARED
	${androidsdrdriverinput_SOURCES}
)

target_link_libraries(${TARGET_NAME}
    Qt::Core
    ${TARGET_LIB}
	sdrbase
	${TARGET_LIB_GUI}
    swagger
)
if(NOT ENABLE_QT6 AND ANDROID)
    target_link_libraries(${TARGET_NAME} Qt::AndroidExtras)
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${INSTALL_FOLDER})

# Install debug symbols
if (WIN32)
    install(FILES $<TARGET_PDB_FILE:${TARGET_NAME}> CONFIGURATIONS Debug RelWithDebInfo DESTINATION ${INSTALL_FOLDER} )
endif()
